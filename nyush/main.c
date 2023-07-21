#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

#define MAX_LEN 1000
int num_jobs = 0;

// Struct for jobs
typedef struct job
{
	pid_t pid;
	char *cmd;
	int status; // 0 = running, 1 = stopped, 2 = terminated
} job;
// Array of jobs
job jobs[100];

void handler(int sig)
{
}

int add_job(int status, int pid, char *cmdStr)
{
	if (WIFSTOPPED(status))
	{
		// add job to jobs array
		if (num_jobs < 100)
		{
			jobs[num_jobs].cmd = (char *)malloc(strlen(cmdStr) + 1);
			strcpy(jobs[num_jobs].cmd, cmdStr);
			jobs[num_jobs].pid = pid;
			num_jobs++;
		}
		else
		{
			fprintf(stderr, "Error: too many jobs\n");
		}
	}
	return 0;
}

int checkExCommand(char *arg)
{
	char *fullPath = (char *)malloc(strlen(arg) + strlen("/usr/bin/") + 1);

	if (arg[0] != '/' && arg[0] != '.' && !(strchr(arg, '/') > 0))
	{
		strcpy(fullPath, "/usr/bin/");
		strcat(fullPath, arg);
		if (access(fullPath, X_OK) != 0)
		{
			// If file does not exist
			fprintf(stderr, "Error: invalid program\n");
			return 1;
		}
	}
	return 0;
}

struct pair
{
	int start;
	int end;
};

struct pair find_interval(int arr[], int size, int interval)
{
	int start_index = -1;
	int end_index = size - 1;
	int count = 0;

	for (int i = 0; i < size; i++)
	{
		if (arr[i] == 1)
		{
			count++;
			if (count == interval)
			{
				start_index = i;
			}
			else if (count == interval + 1)
			{
				end_index = i - 1;
				break;
			}
		}
	}

	if (start_index != -1 && end_index == -1)
	{
		end_index = size - 1;
	}
	start_index++;
	struct pair result = {start_index, end_index};
	return result;
}

int getToNextOp(int op_loc[], int num_ops)
{
	int pipe_end;
	for (int i = 0; i < num_ops; i++)
	{
		if (op_loc[i] == 1)
		{
			pipe_end = i;
		}
	}
	for (int i = 0; i < num_ops; i++)
	{
		if (op_loc[i] > 0 && op_loc[i] < pipe_end)
		{
			return op_loc[i];
		}
	}
	return -1;
}

int main(void)
{
	char *args[MAX_LEN / 2 + 1]; // Command line arguments
	char command[MAX_LEN];		 // The command entered
	bool flag = 1;				 // Flag to determine when to exit program
	bool cont;					 // Flag to determine when to continue
	char wd[100];				 // https://stackoverflow.com/questions/298510/how-to-get-the-current-directory-in-a-c-program
	char *base;					 // https://stackoverflow.com/questions/7180293/how-to-extract-filename-from-path
	// pid_t pid;

	signal(SIGINT, handler);
	signal(SIGQUIT, handler);
	signal(SIGTSTP, handler);

	while (flag)
	{
		// Do nothing with SIGNIT, SIGQUIT, SIGTSTP

		cont = 0;
		// Prompt of directory name
		getcwd(wd, sizeof(wd));
		base = basename(wd);
		printf("[nyush %s]$ ", base);
		fflush(stdout); // Clear buff

		// Load arguments and command
		if (fgets(command, MAX_LEN, stdin) == NULL)
		{
			printf("\n");
			flag = 0;
			break;
		} // https://www.tutorialspoint.com/c_standard_library/c_function_fgets.htm

		// save the command string without the newline
		char *cmdStr = malloc(strlen(command) + 1);
		strcpy(cmdStr, command);
		cmdStr[strlen(command) - 1] = '\0';

		int num_seg = 0;
		args[0] = strtok(command, " \n");
		while (args[num_seg] != NULL)
		{
			num_seg++;
			args[num_seg] = strtok(NULL, " \n");
		}

		if (args[0] == NULL)
		{
			continue;
		}

		// if "exit 1", then print error message and continue
		if (strcmp(args[0], "exit") == 0 && args[1] != NULL)
		{
			fprintf(stderr, "Error: invalid command\n");
			continue;
		}

		// Implementation of exit command
		else if (strcmp(args[0], "exit") == 0)
		{
			// Error if there are still jobs suspended
			if (num_jobs > 0)
			{
				fprintf(stderr, "Error: there are suspended jobs\n");
				continue;
			}
			else
			{
				flag = 0;
				continue;
			}
		}

		// Handle jobs
		else if (strcmp(args[0], "jobs") == 0)
		{
			for (int i = 0; i < num_jobs; i++)
			{
				printf("[%d] %s\n", i + 1, jobs[i].cmd);
			}
			continue;
		}

		// Handle fg
		else if (strcmp(args[0], "fg") == 0)
		{
			if (args[1] == NULL)
			{
				fprintf(stderr, "Error: invalid command\n");
				continue;
			}
			else if (args[2] != NULL)
			{
				fprintf(stderr, "Error: invalid command\n");
				continue;
			}
			else if (atoi(args[1]) - 1 >= num_jobs)
			{
				fprintf(stderr, "Error: invalid job\n");
				continue;
			}
			else
			{
				// print the job number and name
				// printf("[%d] %s\n", atoi(args[1]), jobs[atoi(args[1]) - 1].cmd);

				int status;
				kill(jobs[atoi(args[1]) - 1].pid, SIGCONT);
				waitpid(jobs[atoi(args[1]) - 1].pid, &status, WUNTRACED);
				char *cmdName = (char *)malloc(strlen(jobs[atoi(args[1]) - 1].cmd) + 1);
				strcpy(cmdName, jobs[atoi(args[1]) - 1].cmd);
				int pid = jobs[atoi(args[1]) - 1].pid;

				num_jobs--;
				for (int i = atoi(args[1]) - 1; i < num_jobs; i++)
				{
					jobs[i] = jobs[i + 1];
				}

				// if the job exits, then remove it from the array and decrement num_jobs
				add_job(status, pid, cmdName);
				continue;
			}
		}

		// Implementation of cd command
		else if (strcmp(args[0], "cd") == 0)
		{
			// When no directory is specified
			if (args[1] == NULL)
			{
				fprintf(stderr, "Error: invalid command\n");
				continue;
			}
			// When directory is specified
			else if (chdir(args[1]) != 0)
			{
				fprintf(stderr, "Error: invalid directory\n");
				continue;
			}
			// Avoid executing external commands
			continue;
		}

		// Get the operator locations
		int op_loc[num_seg]; // 0 for no operator, 1 for |, 2 for >, 3 for >>, 4 for <
		int num_op = 0;
		int num_pipes = 0;
		for (int i = 0; args[i] != NULL; i++)
		{
			// printf("The segment is %s\n", args[i]);
			if (strcmp(args[i], "|") == 0)
			{
				op_loc[i] = 1;
				num_op++;
				num_pipes++;
				// error if on the boundary
				if (i == 0 || args[i + 1] == NULL)
				{
					fprintf(stderr, "Error: invalid command\n");
					cont = 1;
					break;
				}
			}
			else if (strcmp(args[i], ">") == 0)
			{
				op_loc[i] = 2;
				num_op++;
				// error if on the boundary
				if (i == 0 || args[i + 1] == NULL)
				{
					fprintf(stderr, "Error: invalid command\n");
					cont = 1;
					break;
				}
			}
			else if (strcmp(args[i], ">>") == 0)
			{
				op_loc[i] = 3;
				num_op++;
				// error if on the boundary
				if (i == 0 || args[i + 1] == NULL)
				{
					fprintf(stderr, "Error: invalid command\n");
					cont = 1;
					break;
				}
			}
			else if (strcmp(args[i], "<") == 0)
			{
				op_loc[i] = 4;
				num_op++;
				// error if on the boundary
				if (i == 0 || args[i + 1] == NULL)
				{
					fprintf(stderr, "Error: invalid command\n");
					cont = 1;
					break;
				}
			}
			else
			{
				op_loc[i] = 0;
			}
		}
		if (cont == 1)
		{
			continue;
		}

		// Preparing pipes
		int fd[num_pipes][2];
		for (int i = 0; i < num_pipes; i++)
		{
			if (pipe(fd[i]) == -1)
			{
				fprintf(stderr, "Error: pipe failed\n");
			}
		}

		// print the pipe information
		// printf("The pipe information is:\n");
		// for (int i = 0; i < num_pipes; i++)
		// {
		// 	printf("fd[%d][0]: %d\n", i, fd[i][0]);
		// 	printf("fd[%d][1]: %d\n", i, fd[i][1]);
		// }

		// printf("The file descriptors that the process is using are:\n");
		// for (int j = 0; j < 8; j++)
		// {
		// 	printf("fd[%d]: %d\n", j, fcntl(j, F_GETFD));
		// }

		// printf("num_op: %d\n", num_op);
		// printf("num_pipes: %d\n", num_pipes);
		// printf("num_seg: %d\n", num_seg);
		// for (int j = 0; j < num_seg; j++)
		// {
		// 	printf("op_loc[%d]: %d\n", j, op_loc[j]);
		// }

		// Check for redirection or pipes
		for (int i = num_pipes; i >= 0; i--)
		{
			// printf("i: %d\n", i);
			int start;
			int end;
			start = find_interval(op_loc, num_seg, i).start;
			end = find_interval(op_loc, num_seg, i).end;

			args[end + 1] = NULL;

			// printf("\nThe command is: %s\n", args[start]);
			// printf("It ends with %s\n", args[end]);
			int child_pid = fork();
			if (child_pid == 0)
			{
				// If in the child process

				// printf("\nThe %dth child process is created\n", i);

				// // print out the file descriptors that the process is using
				// printf("The file descriptors that the process is using are:\n");
				// for (int j = 0; j < 8; j++)
				// {
				// 	printf("fd[%d]: %d\n", j, fcntl(j, F_GETFD));
				// }

				int in_errcode;
				// int out_errcode;
				// Read if it is not the first command
				printf("i: %d, num_pipe: %d\n", i, num_pipes);
				if (i != 0)
				{
					// read from the previous pipe
					printf("reading from the previous pipe");
					fflush(stdin);
					in_errcode = dup2(fd[i - 1][0], STDIN_FILENO);
					if (in_errcode == -1)
					{
						fprintf(stderr, "dup2 failed: %s\n", strerror(errno));
					}
					printf("code: %d", in_errcode);
					fflush(stdin);
					// dup2(fd[i - 1][0], STDIN_FILENO);
				}
				// Write if it is not the last command
				if (i != num_pipes)
				{
					// write to the next pipe
					printf("writing to the next pipe");
					// out_errcode = dup2(fd[i][1], STDOUT_FILENO);
					// if (out_errcode == -1)
					// {
					// 	fprintf(stderr, "dup2 failed: %s\n", strerror(errno));
					// }
					printf("%d", dup2(fd[i][1], STDOUT_FILENO));
				}

				// check if there is redirection before the next pipe
				if (getToNextOp(op_loc, num_op) == 2)
				{
					// redirect stdout to the file
					int fd = open(args[getToNextOp(op_loc, num_op) + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
					dup2(fd, 1);
					close(fd);
				}
				else if (getToNextOp(op_loc, num_op) == 3)
				{
					// redirect stdout to the file
					int fd = open(args[getToNextOp(op_loc, num_op) + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
					dup2(fd, 1);
					close(fd);
				}
				else if (getToNextOp(op_loc, num_op) == 4)
				{
					// redirect stdin to the file
					int fd = open(args[getToNextOp(op_loc, num_op) + 1], O_RDONLY);
					dup2(fd, 0);
					close(fd);
				}

				// Execute the command
				for (int j = 0; j < num_pipes; j++)
				{
					close(fd[j][0]);
					close(fd[j][1]);
				}
				fflush(stdout);
				execvp(args[start], args + start);
				fprintf(stderr, "Error: invalid command\n");
				exit(0);
			}
			else if (child_pid < 0)
			{
				// If error
				fprintf(stderr, "Fork error\n");
			}
			else
			{
				// Parent Process
				int status;
				waitpid(child_pid, &status, WUNTRACED);
				add_job(status, child_pid, cmdStr);
			}
		}

		for (int j = 0; j < num_pipes; j++)
		{
			close(fd[j][0]);
			close(fd[j][1]);
		}

		// // Implementation of external commands
		// if (checkExCommand(args[0]) != 0)
		// {
		// 	continue;
		// }

		// pid = fork();
		// if (pid == 0)
		// {
		// 	// If in the child process
		// 	execvp(args[0], args);
		// 	// printf("Error: invalid external command\n");
		// 	fprintf(stderr, "Error: invalid program\n");
		// 	exit(0);
		// }
		// else if (pid < 0)
		// {
		// 	// If error
		// 	fprintf(stderr, "Fork error\n");
		// }
		// else
		// {
		// 	// Parent Process
		// 	int status;
		// 	waitpid(pid, &status, WCONTINUED | WUNTRACED);
		// 	add_job(status, pid, cmdStr);
		// }
	}
	return 0;
}
