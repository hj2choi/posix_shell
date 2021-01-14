/*
COMP4511 assignment 2
Author: CHOI, Hong Joon

TODO use dynamic array

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>

#define MAX_CMDLINE_LEN 256
#define MAX_ARGV_LEN 30


/* function prototypes go here... */
void show_prompt();
int get_cmd_line(char *cmdline);
void process_cmd(char *cmdline);


void print_argv(const char* const argv[], const char* const name) {
	int i;
	printf("%s = ", name);
	for (i=0; argv[i]!=NULL; ++i) {
		printf("%s ",argv[i]);
	}
	printf("\n");
}

void split(char* out_str[], char* input, const char* seperator) {
	char* token;
	token = strtok(input, seperator);
	//split string
	int i = 0;
	while( token != NULL )
  {
    //printf( "%s\n", token );
		out_str[i++] = token;
  	token = strtok(NULL, seperator);
  }
	out_str[i] = NULL;
}

int get_argv(int* run_process_in_background, char* program_argv[], char* full_argv[], char* str_in, const char* seperator) {

	split(full_argv, str_in, seperator);

	// make a copy of full_argv into program_argv
	int i;
	for (i=0; full_argv[i]!=NULL; ++i) {
		program_argv[i]=full_argv[i];
	}
	program_argv[i]=NULL;

	// prune program_argv by searching for signals ["|", "<", ">"]
	int prune_flag=0;
	int program_argv_len=0;
	for (i=0;program_argv[i]!=NULL;++i) {
		if (!prune_flag)
			program_argv_len = i;
		if (!strcmp(program_argv[i], "|") || !strcmp(program_argv[i], ">") || !strcmp(program_argv[i], "<")) {
			prune_flag=1;
		}
		if (prune_flag)
			program_argv[i]=NULL;
	}
	if (!prune_flag)
		program_argv_len++;

	// run in background command
	*run_process_in_background = 0;
	if (program_argv_len>0 && program_argv[program_argv_len-1]) {
		if (program_argv[program_argv_len-1][0]=='&') {
			program_argv[program_argv_len-1]=NULL;
			*run_process_in_background = 1;
		}
	}

	// no argument
	if (!program_argv[0]) {
		return 1;
	}

	return 0;
}

void get_program_argv(char* program_argv[], char* const full_argv[]) {
	int i;
	for (i=0; full_argv[i]!=NULL; ++i) {
		program_argv[i] = full_argv[i];
	}
	program_argv[i] = NULL;

	for (i=0;program_argv[i]!=NULL;++i) {
		if (!strcmp(program_argv[i], "|") || !strcmp(program_argv[i], ">") || !strcmp(program_argv[i], "<")) {
			program_argv[i]=NULL;
		}
	};
}

// redirect IO, by traversing full_argv, with signals ["|", "<", ">"]
int redirect_IO(int* pipe_flag, char* full_argv[], char* post_pipe_full_argv[], int const pipe_fds[], int pipe_stdin_flag) {
	*pipe_flag=0;
	int i;

	//redirect stdin to pipe's read end
	if (pipe_stdin_flag) {
		//printf("redirect pipe to stdout %d\n",pipe_fds[1]);
		close(1);	// close stdout
		dup(pipe_fds[1]);	// redirect pipe write pin to stdout
		close(pipe_fds[0]);
	}

	// traverse full_argv, in reverse direction
	for (i=0; full_argv[i]!=NULL; ++i);

	for (--i; i>=0; --i) {
		//pipe, raise the pipe flag, compute remainder, and return
		if (!strcmp(full_argv[i], "|")) {
			*pipe_flag = 1;
			int remainder_i=0;
			full_argv[i++]=NULL;
			for (;full_argv[i]!=NULL; ++i) {
				post_pipe_full_argv[remainder_i++] = full_argv[i];
				full_argv[i]=NULL;
			}
			post_pipe_full_argv[remainder_i]=NULL;
			return 0;
		}

		// redirect stdout
		if (!strcmp(full_argv[i], ">")) {
			if (full_argv[i+1]){
				//printf("opening file %s\n", full_argv[i+1]);
				int writeFd = open(full_argv[i+1], O_WRONLY| O_TRUNC | O_CREAT, 0777);
				if (writeFd==-1) {
					printf(">: error opening file\n");
					return -1;
				}
				//printf("redirect stdout to fd %d\n",writeFd);
				close(1);// close stdout
				dup2(writeFd, 1); // redirect stdout to file
			}
			else {
				printf(">: target file not specified\n");
			}
		}

		// redirect stdin
		if (!strcmp(full_argv[i], "<")) {
			if (full_argv[i+1]){
				//printf("opening file %s\n", full_argv[i+1]);
				int readFd = open(full_argv[i+1], O_RDONLY, 0777);
				if (readFd==-1) {
					printf("<: error opening file\n");
					return -1;
				}
				//printf("redirect stdin to fd %d\n",readFd);
				close(0);// close stdin
				dup2(readFd, 0); // redirect stdout to file
			}
			else {
				printf("<: target file not specified\n");
			}

		}
	}
	return 0;
}


void fork_and_execute(char* program_argv[], char* full_argv[], int run_in_background, int pipe_stdout_flag)
{

	int pipe_fds[2];
	pipe(pipe_fds);
	pid_t pid = fork();

	if (pid==0) {		// child process

		int pipe_flag;
		char* post_pipe_full_argv[MAX_ARGV_LEN];
		if (redirect_IO(&pipe_flag, full_argv, post_pipe_full_argv, pipe_fds, pipe_stdout_flag)) {
			printf("error\n");
			return;
		}
		if (pipe_flag) {
			char* child_program_argv[MAX_ARGV_LEN];
			get_program_argv(child_program_argv, full_argv); // get remainder to proceed to children
			fork_and_execute(child_program_argv, full_argv, 0, 1);

			get_program_argv(program_argv, post_pipe_full_argv); // this will change file to execute
		}
		int retval = execvp(program_argv[0],program_argv);
		if (retval==-1) {
			printf("%s: %s\n",program_argv[0],"cannot be found");
		}
		//printf("%s	\n","child process terminating...");
		exit(0);
  }
  else {	// parent process
		if (pipe_stdout_flag) {
			close(0);	// close stdin
			dup(pipe_fds[0]);	// redirect pipe read pin to stdin
			close(pipe_fds[1]); // close write pin of pipe
			//printf("redirect pipe to stdin %d\n",pipe_fds[0]);
		}
		if (!run_in_background)	// parent waits when you don't run in background
			wait(0);
		//printf("%s	\n","child process continuing...");
  }
}



int execute_shell_command(char* const program_argv[], int* run_in_background) {

	//exit command
	if (!strcmp(program_argv[0], "exit")) {
		printf("myshell is terminated with pid %d\n", getpid());
		exit(0);
		return 0;
	}

	//cd command
	if (!strcmp(program_argv[0], "cd")) {
		if (*run_in_background)
			return 0;
		char cwd[MAX_CMDLINE_LEN];

		if (program_argv[1]) {
			if (!strcmp(program_argv[1], "/")) {
				strcpy(cwd, "/");	// cd /
			}
			else {
				getcwd(cwd, sizeof(cwd));	// cd [dir], cd ..
				strcat(cwd, "/");
				strcat(cwd, program_argv[1]);
			}
		}
		else {
			strcpy(cwd, "/home");	// cd ~
		}

		if (chdir(cwd)) {
			printf("%s\n",strerror(errno));
			return -1;
		}
		return 0;
	}
	return 1;
}

void process_cmd(char *cmdline)
{
	char* program_argv[MAX_ARGV_LEN];
	char* full_argv[MAX_ARGV_LEN];
	int run_in_background;

	if (get_argv(&run_in_background, program_argv, full_argv, cmdline, " "))
		return;
	if (execute_shell_command(program_argv, &run_in_background)<=0)
		return;
	fork_and_execute(program_argv, full_argv, run_in_background, 0);
}

/* The main function implementation */
int main()
{
	char cmdline[MAX_CMDLINE_LEN];
	printf("\nbaby shell written by Hong Joon CHOI (20161472, hjchoi@connect.ust.hk)\n");
	printf("please seperate each arguments by space\n\n\n");

	while (1)
	{
		show_prompt();
		if ( get_cmd_line(cmdline) == -1 )
			continue; /* empty line handling */

		process_cmd(cmdline);
	}
	return 0;

}

void show_prompt()
{
	char cwd[1024];
	char* current_dir[256];
	getcwd(cwd, sizeof(cwd));
	split(current_dir, cwd, "/");
	int i;
	for (i=0;current_dir[i]!=NULL;++i);

	printf("[");
	if (current_dir[i-1])
		printf("%s",current_dir[i-1]);
	printf("] myshell> ");
}

int get_cmd_line(char *cmdline)
{
  int i;
  int n;
  if (!fgets(cmdline, MAX_CMDLINE_LEN, stdin))
      return -1;
  // Ignore the newline character
  n = strlen(cmdline);
  cmdline[--n] = '\0';
  i = 0;
  while (i < n && cmdline[i] == ' ')
  {
      ++i;
  }
  if (i == n)
  {
      // Empty command
      return -1;
  }
  return 0;
}
