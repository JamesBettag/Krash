#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

int main(int argc, char *argv[])
{
	pid_t pid;
	char input[256], *token, *tmp;
	char **args;
	size_t size;
	bool append = false, redirection = false, duplicateRedirection = false;
	int numArgs = 0, redirectPlace;
	int i, j;
	int fd, save_out;

	if( argc > 2)
	{
		//check for invalid usage, then exit
		fprintf(stderr, "Usage: ./krash [-h]\n");
		exit(1);
	}
	if(argc == 2)
	{
		// check if second arg is -h
		if(strcmp(argv[1], "-h") == 0)
		{
			// exec to cat out krashHelp.txt
			char *catArgs[3];
			catArgs[0] = strdup("cat");
			catArgs[1] = strdup("krashHelp.txt");
			catArgs[2] = NULL;
			execvp(catArgs[0], catArgs);
			exit(0);
		}
		fprintf(stderr, "Usage: ./krash [-h]\n");
		exit(1);
	}
	else
	{   // endlessly loop to accept user input
		while(true)
		{
			redirectPlace = 0;
			printf("krash: ");
			fgets(input, 256, stdin);
			// get rid of newline at the end
			i = 0;
			while(input[i] != '\0')
			{
				if(input[i] == '\n')
				{
					input[i] = '\0';
				}
				i++;
			}
			// duplicate input for strtok, then count num of args
			i = 0;
			numArgs = 0;
			tmp = strdup(input);
			token = strtok(tmp, " ");
			while(token != NULL) {
				// check if redirection is present
				if(strcmp(token, ">") == 0)
				{
					// check if redirection was previously set
					if(redirection == false)
					{
						redirection = true;
						redirectPlace = numArgs;
					}
					else // if it was previously set, invalid redirection
					{
						redirection = false;
						duplicateRedirection = true;
					}
				}
				//check if appended redirection is present
				else if(strcmp(token, ">>") == 0)
				{
					if(redirection == false) {
						redirection = true;
						append = true;
						redirectPlace = numArgs;
					}
					else
					{
						redirection = false;
						duplicateRedirection = true;
					}
				}
				numArgs++;
				token = strtok(NULL, " ");
			}

			if(duplicateRedirection)
			{
				// user inputed invalid use of redirection
				duplicateRedirection = false;
				printf("Invalid Use of Redirection\n");
				// skip to next loop iteration
				continue;
			}
			else if(numArgs == 0)
			{
				// check if just enter was pressed
				// if true, skip to next loop iteration
				continue;
			}
			// allocate dynamic memory for arguments
			// must account for size to add NULL for exec
			args = (char **)malloc(sizeof(char *) * (numArgs + 1));
			// place all args in dynamic array
			tmp = strdup(input);
			token = strtok(tmp, " ");
			i = 0;
			while(token != NULL)
			{
				args[i] = strdup(token);
				token = strtok(NULL, " ");
				i++;
			}
			// append NULL to the end of array
			args[i] = NULL;
			// handle redirection
			if(redirection)
			{
				// check if there is a file to redirect to
				if((redirectPlace > 0) && (redirectPlace == (numArgs - 2)))
				{
					if(!append)
					{
						//open file descriptor to provided filename with truncate options
						fd = open(args[redirectPlace + 1], O_WRONLY | O_TRUNC | O_CREAT);
					}
					else
					{
						//open file descriptor to provided filename with appending options
						fd = open(args[redirectPlace + 1], O_WRONLY | O_APPEND | O_CREAT);
					}
					if(fd != -1)
					{
						// save stdout file descriptor
						save_out = dup(fileno(stdout));
						// change stdout output to file, then check if success
						if(dup2(fd, fileno(stdout)) == -1)
						{
							printf("Could not redirect output\n");
							redirection = false;
							append = false;
						}
						else
						{
							// put NULL in the place of "<<" or "<" in args
							args[redirectPlace] = NULL;
						}
					}
				}
				else
				{
					// if no file to redirect to, continue to next loop iteration
					printf("Invalid Use of Redirection\n");
					redirection = false;
					continue;
				}
			}
			//check for built in commands
			// check for exit command
			if(strcmp(args[0], "exit") == 0)
			{
				printf("Bye!\n");
				exit(0);
			}
			// check for pwd command
			else if(strcmp(args[0], "pwd") == 0)
			{
				char workingPath[PATH_MAX];
				if(getcwd(workingPath, sizeof(workingPath)) != NULL)
				{
					printf("%s\n", workingPath);
				}
				else
				{
					printf("Could not locate path.\n");
				}
			}
			// check for help command
			else if(strcmp(args[0], "help") == 0)
			{
				FILE *fp = fopen("krashHelp.txt", "r");
				char tmp;
				if(fp == NULL) {
					fprintf(stderr, "Help file Missing. Terminating.\n");
					exit(1);
				}
				else
				{
					while((tmp = fgetc(fp)) != EOF)
					{
						putchar(tmp);
					}
					fclose(fp);
				}
			}
			//check for cd command
			else if(strcmp(args[0], "cd") == 0)
			{
				//printf("%d\n", numArgs);
				if(numArgs > 2)
				{
					printf("cd Usage: cd [dir]\n");
				}
				else if(numArgs == 1)
				{
					chdir(getenv("HOME"));
				}
				else
				{
					if((chdir(args[1])) == -1)
					{
						printf("Invalid Directory %s\n", args[1]);
					}
					else
					{
						char workingPath[PATH_MAX];
						// get working path from getcwd
						if(getcwd(workingPath, sizeof(workingPath)) != NULL)
						{
							printf("Current Path: %s\n", workingPath);
						}
						else
						{
							printf("Could not locate path.\n");
						}
					}
				}
			}
			// fork and call exec for non built-in command
			else
			{
				pid = fork();
				if(pid == 0)
				{
					// call exec to handle args
					execvp(args[0], args);
					// if exec returns, it could not locate arg[0]
					printf("Invalid Command %s\n", args[0]);
					exit(0);
				}
				else
				{
					// parent waits for child to terminate
					wait(NULL);
				}
			}
			// reset stdout to its initial file descriptor
			if(redirection == true)
			{
				// flush stdout
				fflush(stdout);
				//close file
				close(fd);
				// return stdout to its initial file descriptor value
				dup2(save_out, fileno(stdout));
				close(save_out);
			}
			append = false;
			redirection = false;
			numArgs = 0;
			// deallocate memory
			free(args);

		}
	}
	return 0;
}
