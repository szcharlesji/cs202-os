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


int outputRedirection(char *args[])
{
	
		fflush(stdout);
		pid_t pid = fork();
		// Execute the command before the redirection
		if (pid == 0)
		{
			// If in the child process
			int fd;
			if (strcmp(args[j], ">") == 0)
			{
				fd = open(args[j + 1], O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
			}
			else
			{
				fd = open(args[j + 1], O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR);
			}
			if (fd < 0)
			{ // Error opening file
				fprintf(stderr, "Error: failed to open file\n");
				break;
			}
			dup2(fd, 1);
			close(fd);
			args[j] = NULL;
			execvp(args[0], args);
			fprintf(stderr, "Error: invalid command\n");
			exit(0);
		}
		else
		{
			// Parent Process
			int status;
			waitpid(pid, &status, WUNTRACED);
			add_job(status, pid, cmdStr);
		}
	
}
