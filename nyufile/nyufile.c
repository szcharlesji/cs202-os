// "Copyright 2023 Cheng Ji"
// Description: This program is used to recover files from a disk image.
// The disk image is a file that contains a file system.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <openssl/sha.h>

#define SHA_DIGEST_LENGTH 20

// Taken from the slides
#pragma pack(push, 1)
typedef struct BootEntry
{
	unsigned char BS_jmpBoot[3];	// Assembly instruction to jump to boot code
	unsigned char BS_OEMName[8];	// OEM Name in ASCII
	unsigned short BPB_BytsPerSec;	// Bytes per sector. Allowed values include 512, 1024, 2048, and 4096
	unsigned char BPB_SecPerClus;	// Sectors per cluster (data unit). Allowed values are powers of 2, but the cluster size must be 32KB or smaller
	unsigned short BPB_RsvdSecCnt;	// Size in sectors of the reserved area
	unsigned char BPB_NumFATs;		// Number of FATs
	unsigned short BPB_RootEntCnt;	// Maximum number of files in the root directory for FAT12 and FAT16. This is 0 for FAT32
	unsigned short BPB_TotSec16;	// 16-bit value of number of sectors in file system
	unsigned char BPB_Media;		// Media type
	unsigned short BPB_FATSz16;		// 16-bit size in sectors of each FAT for FAT12 and FAT16. For FAT32, this field is 0
	unsigned short BPB_SecPerTrk;	// Sectors per track of storage device
	unsigned short BPB_NumHeads;	// Number of heads in storage device
	unsigned int BPB_HiddSec;		// Number of sectors before the start of partition
	unsigned int BPB_TotSec32;		// 32-bit value of number of sectors in file system. Either this value or the 16-bit value above must be 0
	unsigned int BPB_FATSz32;		// 32-bit size in sectors of one FAT
	unsigned short BPB_ExtFlags;	// A flag for FAT
	unsigned short BPB_FSVer;		// The major and minor version number
	unsigned int BPB_RootClus;		// Cluster where the root directory can be found
	unsigned short BPB_FSInfo;		// Sector where FSINFO structure can be found
	unsigned short BPB_BkBootSec;	// Sector where backup copy of boot sector is located
	unsigned char BPB_Reserved[12]; // Reserved
	unsigned char BS_DrvNum;		// BIOS INT13h drive number
	unsigned char BS_Reserved1;		// Not used
	unsigned char BS_BootSig;		// Extended boot signature to identify if the next three values are valid
	unsigned int BS_VolID;			// Volume serial number
	unsigned char BS_VolLab[11];	// Volume label in ASCII. User defines when creating the file system
	unsigned char BS_FilSysType[8]; // File system type label in ASCII
} BootEntry;
#pragma pack(pop)

// Taken from the slides
#pragma pack(push, 1)
struct Entry
{
	unsigned char filename[8];
	unsigned char ext[3];
	unsigned char fileAttributes;
	unsigned char reserved;
	unsigned char createTimeFine;
	unsigned short createTime;
	unsigned short createDate;
	unsigned short lastAccessDate;
	unsigned short clusterAddressHigh;
	unsigned short modifiedTime;
	unsigned short modifiedDate;
	unsigned short clusterAddressLow;
	unsigned int fileSize;
} Entry;
#pragma pack(pop)

// Print Usage information
void printUsage()
{
	printf("Usage: ./nyufile disk <options>\n");
	printf("  -i                     Print the file system information.\n");
	printf("  -l                     List the root directory.\n");
	printf("  -r filename [-s sha1]  Recover a contiguous file.\n");
	printf("  -R filename -s sha1    Recover a possibly non-contiguous file.\n");
}

// Function to print the file system information
void printFileSystemInfo(char *filename)
{
	// print: number of FATs, bytes per sector, sectors per cluster, reserved sectors
	int numFATs;
	int bytesPerSector;
	int sectorsPerCluster;
	int reservedSectors;

	// Seek to the beginning of the boot sector
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "Error: Unable to open file %s.\n", filename);
		return;
	}
	fseek(fp, 0, SEEK_SET);

	// Read the boot sector
	unsigned char boot[512];
	fread(boot, 1, 512, fp);

	// Print the file system information
	numFATs = boot[16];
	bytesPerSector = boot[11] + boot[12] * 256; // Corrected byte order
	sectorsPerCluster = boot[13];
	reservedSectors = boot[14] + boot[15] * 256; // Corrected byte order

	printf("Number of FATs = %d\n", numFATs);
	printf("Number of bytes per sector = %d\n", bytesPerSector);
	printf("Number of sectors per cluster = %d\n", sectorsPerCluster);
	printf("Number of reserved sectors = %d\n", reservedSectors);

	fclose(fp);
}

// Function to list the root directory
void listRootDirectoryCont(char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "Error: Unable to open file %s.\n", filename);
		return;
	}

	// load boot sector to BootEntry struct
	BootEntry boot;
	fread(&boot, sizeof(boot), 1, fp);

	// load the FAT table
	// Taken from the slides
	uint16_t bytes_per_sector = boot.BPB_BytsPerSec;
	uint32_t reserved_sectors = boot.BPB_RsvdSecCnt;
	uint32_t sectors_per_fat = boot.BPB_FATSz32;
	uint32_t fat_size = bytes_per_sector * sectors_per_fat;
	uint32_t fat_offset = bytes_per_sector * reserved_sectors;

	int fatEntries = fat_size / 4;
	int fat[fatEntries];
	fseek(fp, fat_offset, SEEK_SET);
	fread(&fat, sizeof(fat), 1, fp);

	// Read the root directory
	struct Entry entry;
	int entryCount = 0;
	int clusterSize = boot.BPB_SecPerClus * boot.BPB_BytsPerSec;
	int numEntries = clusterSize / sizeof(struct Entry);
	int currentCluster = boot.BPB_RootClus;
	int rootCluster = currentCluster;

	// Seek to the beginning of the root directory
	// get offset of root directory
	int offset = boot.BPB_RsvdSecCnt * boot.BPB_BytsPerSec + boot.BPB_NumFATs * boot.BPB_FATSz32 * boot.BPB_BytsPerSec;
	offset += (currentCluster - 2) * clusterSize;
	// int first_data_sector = boot.BPB_RsvdSecCnt + (boot.BPB_NumFATs * boot.BPB_FATSz32);
	// int offset = (first_data_sector + (boot.BPB_RootClus - 2) * boot.BPB_SecPerClus) * boot.BPB_BytsPerSec;

	// get sector size
	int sectorSize = boot.BPB_BytsPerSec;
	// get number of entries per sector
	int entriesPerSector = sectorSize / sizeof(struct Entry);

	// Seek to the beginning of the root directory

	fseek(fp, offset, SEEK_SET);

	// while (fread(&entry, sizeof(entry), 1, fp) == 1 && entry.filename[0] != 0x00)
	for (int i = 0; i < numEntries; i++)
	// for (int i = 0; i < entriesPerSector; i++)
	{
		// Read the entry
		if (fread(&entry, sizeof(entry), 1, fp) != 1)
		{
			break;
		}

		if (entry.fileAttributes == 0x10 || entry.fileAttributes == 0x20)
		{
			// Check if the entry is not a deleted file (first byte of the filename is not 0xE5)
			if (entry.filename[0] != 0xE5)
			{
				entryCount++;

				bool isDirectory = (entry.fileAttributes & 0x10) != 0;
				bool isFile = (entry.fileAttributes & 0x20) != 0;

				uint32_t startingCluster = (entry.clusterAddressHigh << 16) | entry.clusterAddressLow;
				uint32_t size = entry.fileSize;

				char ext[4];
				strncpy(ext, entry.ext, 3);
				ext[3] = '\0';

				// stop the file name at the first space, if there is no space, stop at the end of the filename
				for (int i = 0; i < 8; i++)
				{
					if (entry.filename[i] == ' ')
					{
						entry.filename[i] = '\0';
						break;
					}
					else if (i == 7)
					{
						entry.filename[i + 1] = '\0';
					}
				}

				// stop the extension at the first space
				for (int i = 0; i < 3; i++)
				{
					if (ext[i] == ' ')
					{
						ext[i] = '\0';
						break;
					}
				}

				if (isDirectory)
				{
					printf("%s/ (starting cluster = %u)\n", entry.filename, startingCluster);
				}
				else if (isFile)
				{
					// if the file size is 0, print the file name
					if (size == 0)
					{
						if (ext[0] == '\0')
						{
							printf("%s (size = 0)\n", entry.filename);
						}
						else
						{
							printf("%s.%s (size = 0)\n", entry.filename, ext);
						}
					}
					// if there is no extension, print the file name
					else if (ext[0] == '\0')
					{
						printf("%s (size = %u, starting cluster = %u)\n", entry.filename, size, startingCluster);
					}
					// if there is an extension, print the file name and extension
					else
					{
						printf("%s.%s (size = %u, starting cluster = %u)\n", entry.filename, ext, size, startingCluster);
					}
				}
			}
			// If it is the last entry, check FAT for the next entry.
			if (i == numEntries - 1 && currentCluster != fatEntries - 1 && fat[currentCluster] != 0x0FFFFFFF)
			{
				// go to the next cluster in the FAT table
				// currentCluster = fat[currentCluster];
				offset += (fat[currentCluster] - currentCluster) * clusterSize;
				currentCluster = fat[currentCluster];
				// seek to the next cluster
				fseek(fp, offset, SEEK_SET);
				// reset the entry counter
				i = -1;
			}
		}
	}

	printf("Total number of entries = %d\n", entryCount);
	fclose(fp);
}

// Function to recover a contiguous file
void recoverContiguousFile(char *diskName, char *filename, char *sha_hash)
{

	int offset = getOffset(diskName);

	// Seek to the beginning of the root directory
	FILE *fp = fopen(diskName, "r+b");
	if (fp == NULL)
	{
		fprintf(stderr, "Error: Unable to open file %s.\n", diskName);
		return;
	}
	fseek(fp, offset, SEEK_SET);

	// Read the root directory
	struct Entry entry;
	int numFound = 0;
	int foundBySha = 0;
	long entryPosition;

	// load boot sector
	struct BootEntry bootSector;
	fseek(fp, 0, SEEK_SET);
	fread(&bootSector, sizeof(bootSector), 1, fp);

	// load FAT table
	int fatSize = bootSector.BPB_FATSz32 * bootSector.BPB_BytsPerSec;
	int fatOffset = bootSector.BPB_RsvdSecCnt * bootSector.BPB_BytsPerSec;

	// Calculate the number of FAT entries and allocate memory for the FAT table
	int numFatEntries = fatSize / sizeof(uint32_t);
	uint32_t *fatTable = malloc(fatSize);

	// Read the FAT table
	fseek(fp, fatOffset, SEEK_SET);
	fread(fatTable, fatSize, 1, fp);

	uint32_t startingCluster;
	uint32_t size;

	// SHA1 variables
	unsigned char *file_buffer = NULL;
	unsigned char calculated_sha[SHA_DIGEST_LENGTH];

	while (1)
	{
		entryPosition = ftell(fp);

		if (fread(&entry, sizeof(entry), 1, fp) != 1)
		{
			break;
		}

		if (entry.filename[0] != 0xE5 || entry.reserved != 0x0)
		{
			continue;
		}

		startingCluster = (entry.clusterAddressHigh << 16) | entry.clusterAddressLow;
		size = entry.fileSize;

		char ext[4];
		strncpy(ext, entry.ext, 3);
		ext[3] = '\0';

		// stop the file name at the first space, if there is no space, stop at the end of the filename
		for (int i = 0; i < 8; i++)
		{
			if (entry.filename[i] == ' ')
			{
				entry.filename[i] = '\0';
				break;
			}
			else if (i == 7)
			{
				entry.filename[i + 1] = '\0';
			}
		}

		// stop the extension at the first space
		for (int i = 0; i < 3; i++)
		{
			if (ext[i] == ' ')
			{
				ext[i] = '\0';
				break;
			}
		}

		// // put the file name and extension together
		if (ext[0] != '\0')
		{
			strcat(entry.filename, ".");
			strcat(entry.filename, ext);
		}

		// recover empty file
		if (sha_hash)
		{
			char *empty = "da39a3ee5e6b4b0d3255bfef95601890afd80709";
			if (strcmp(entry.filename + 1, filename + 1) == 0 && strncmp(sha_hash, empty, 20) == 0 && size == 0)
			{
				numFound += 1;
				foundBySha = 1;

				// Save back the first char of the filename in the disk image
				char firstChar = filename[0];
				fseek(fp, entryPosition, SEEK_SET);
				fwrite(&firstChar, 1, 1, fp);

				break;
			}
		}

		// if after the first char the filename matches the file to be recovered
		if (strcmp(entry.filename + 1, filename + 1) == 0)
		{

			numFound += 1;
			file_buffer = malloc(size);

			// Save back the first char of the filename in the disk image
			char firstChar = filename[0];
			fseek(fp, entryPosition, SEEK_SET);
			fwrite(&firstChar, 1, 1, fp);

			// if the file size is 0, don't write anything to the disk image
			if (size == 0)
			{
				break;
			}
			// Change the FAT entry to EOF if the file size is less than the cluster size
			// fatTable[startingCluster] = 0x0FFFFFFF;
			if (size <= bootSector.BPB_SecPerClus * bootSector.BPB_BytsPerSec)
			{
				fatTable[startingCluster] = 0x0FFFFFFF;
			}
			else
			{
				int numClusters = size / (bootSector.BPB_SecPerClus * bootSector.BPB_BytsPerSec);

				for (int i = 0; i < numClusters; i++)
				{
					fatTable[startingCluster] = startingCluster + 1;
					startingCluster++;
				}

				fatTable[startingCluster] = 0x0FFFFFFF;
			}

			// write the FAT table back to the disk image
			fseek(fp, fatOffset, SEEK_SET);
			fwrite(fatTable, fatSize, 1, fp);
		}
	}

	if (sha_hash)
	{
		printf("%s: successfully recovered with SHA-1\n", filename);
	}
	else if (numFound == 0)
	{
		fprintf(stderr, "%s: file not found\n", filename);
	}

	else if (numFound == 1)
	{
		printf("%s: successfully recovered\n", filename);
	}
	else
	{
		fprintf(stderr, "%s: multiple candidates found\n", filename);
	}

	fclose(fp);
}

// Function to recover a fragmented file
void recoverFragmentedFile(FILE *fp, char *filename, char *sha1)
{
}

// Function to get the offset of the root directory
// Deprecated function, should've used the boot sector to get the offset
int getOffset(char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
	{
		printf("Error opening file.\n");
		return 1;
	}

	uint16_t bytes_per_sector;
	uint16_t reserved_sectors;
	uint8_t num_FATs;
	uint32_t sectors_per_FAT;

	fseek(fp, 0x0B, SEEK_SET);
	fread(&bytes_per_sector, sizeof(uint16_t), 1, fp);

	fseek(fp, 0x0E, SEEK_SET);
	fread(&reserved_sectors, sizeof(uint16_t), 1, fp);

	fseek(fp, 0x10, SEEK_SET);
	fread(&num_FATs, sizeof(uint8_t), 1, fp);

	fseek(fp, 0x24, SEEK_SET);
	fread(&sectors_per_FAT, sizeof(uint32_t), 1, fp);

	uint32_t root_directory_offset = (reserved_sectors + (num_FATs * sectors_per_FAT)) * bytes_per_sector;

	fclose(fp);
	return root_directory_offset;
}

int main(int argc, char *argv[])
{
	int ch;
	int flag = 0;
	char *sha1 = NULL;
	char *recover_filename = NULL;

	if (argc <= 2)
	{
		printUsage();
		return 1;
	}

	while ((ch = getopt(argc, argv, "ilr:R:s:")) != -1)
	{
		switch (ch)
		{
		case 'i':
			flag = 1;
			// Print the file system information
			printFileSystemInfo(argv[1]);
			break;
		case 'l':
			flag = 1;
			// List the root directory
			listRootDirectoryCont(argv[1]);
			break;
		case 'r':
			flag = 1;
			recover_filename = optarg;
			break;
		case 's':
			flag = 1;
			sha1 = optarg;
			break;
		case 'R':
			flag = 1;
			// Recover a fragmented file
			break;
		default:
			printUsage();
			return 1;
		}
	}

	if (flag == 0 || optind >= argc)
	{
		printUsage();
		return 1;
	}

	// Call recoverContiguousFile after the loop if recover_filename is not NULL
	if (recover_filename != NULL)
	{
		recoverContiguousFile(argv[optind], recover_filename, sha1);
	}

	return 0;
}