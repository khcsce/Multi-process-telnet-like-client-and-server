//NAME: Khoa Quach
//EMAIL: khoaquachschool@gmail.com
//ID: 105123806
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
// new header files for Project 1a:
#include <termios.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
void print_error(const char* msg);
void reset();
void set();
void write_with_check(int fd, const void *buf, size_t size);
void read_write(); 
ssize_t read_with_check(int fd, void *buf, size_t size);
void shell_exit();
void make_pipe(int p[2]);
void signal_handler(int sig);
void check_dup2(int old, int new);
void check_close(int fd);
void child_case();
void parent_case();
/*
Summary of exit codes:
0 ... normal execution, shutdown on ^D
1 ... unrecognized argument or system call failure
*/
/*--------------------------Globals--------------------*/
// For reassurance use hex values too
#define CTRLD 0x04
#define CTRLC 0x03
#define CR 0x0D
#define LF 0x0A
// map received <cr> or <lf> into <cr><lf> 
const char crlf[2] = {'\r','\n'};
//const char crlf [2] = {CR,LF};
//When your program reads a ^C (0x03) from the keyboard, 
// it should use kill(2) to send a SIGINT to the shell
char arrow_C[2] = "^C"; 
// Upon receiving an EOF (^D, or 0x04) from the terminal, close the pipe to the shell,
//  but continue processing input from the shell.
char arrow_D[2] = "^D";
/*----- Passing input and output between two processes => pipe -----*/
int p_to_c[2]; // to the child // parent pipe
int c_to_p[2]; // to the parent // child pipe
pid_t pid; // will be set to fork for creating a new process 
struct termios t0;
/*-------------main------------------------------*/
int main(int argc, char **argv){
	//set();// set up input mode // rest() is used inside set()
	int shell_flag = 0;
	int choice;
	static struct option long_options[] = 
	{
		{"shell", 0, 0, 's'}, // no argument
		{0,0,0,0}
	};
	while((choice = getopt_long(argc, argv, "", long_options,NULL))!= -1)
	{
		switch(choice){
			case 's':
				shell_flag = 1;
				break;
			case '?':
				fprintf(stderr, "unrecognized argument! Correct usage: ./lab1a --shell (--shell is optional)\n");
				exit(1);
			default:
				fprintf(stderr, "unrecognized argument! Correct usage: ./lab1a --shell (--shell is optional)\n");
				exit(1);
		}// switch
	}// while
	set();
	if (shell_flag)
	{
		make_pipe(c_to_p);
		make_pipe(p_to_c);
		atexit(shell_exit); // reassurance if signal handler doesn't work as intender 
		signal(SIGPIPE, signal_handler);
		signal(SIGINT, signal_handler);
		pid = fork(); // create new process => child
		if (pid == -1)
			print_error("fork() error");
		if (pid == 0){
			child_case();
		}
		else{
			parent_case();
		}
	}
	// if --shell option was not used, just do regular read_write();
	read_write();
	//return 0;
	exit(0);
}
/*---------error message helper function --------*/
void print_error(const char* msg){
	fprintf(stderr, "%s , error number: %d, strerror: %s\n ",msg, errno, strerror(errno));
    exit(1);
}
/*-----------input mode non canonical setting ----------*/
/*
put the keyboard (the file open on file descriptor 0) into character-at-a-time, 
no-echo mode (also known as non-canonical input mode with no echo). 
it is suggested that you get the current terminal modes, save them for restoration, 
and then make a copy with only the following changes:*/
// https://www.gnu.org/software/libc/manual/html_node/Noncanon-Example.html
// but I used TCSANOW instead of TCSAFLUSH
void reset(){
	if (tcsetattr(0, TCSANOW, &t0) < 0)
		print_error("Error: could not restore the terminal attributes");
}

void set(){
	struct termios t1;
	// Double check
	int status = isatty(STDIN_FILENO);
	if (!status){
		print_error("This is not a terminal!");
	}
	// save
	tcgetattr(0,&t0);
	atexit(reset); // THIS IS CRUCIAL, saves you from repeating code
	// Set new
	tcgetattr(0,&t1);
	t1.c_iflag = ISTRIP;	/* only lower 7 bits*/
	t1.c_oflag = 0;		/* no processing	*/
	t1.c_lflag = 0;		/* no processing	*/
	t1.c_cc[VMIN] = 1;
	t1.c_cc[VTIME] = 0;
	// new
	if(tcsetattr(0,TCSANOW, &t1) < 0){
		print_error("Error: setting new attributes!");
	}
}
/*-----------------------Regular writing (no --shell) ----------------*/
// spec suggests using 256 bytes,inspired from my function in lab0
// saves time rewriting for the checking if there's a write error
void write_with_check(int fd, const void *buf, size_t size){
	if(write(fd,buf,size) < 0){ 
		print_error("Error: writing error");
	} // Note to self: when passing in as function, it is just the name not &buf
    // size_t write (int fd, void* buf, size_t cnt); 
} // https://www.geeksforgeeks.org/input-output-system-calls-c-create-open-close-read-write/

ssize_t read_with_check(int fd, void *buf, size_t size)
{
	ssize_t s = read(fd,buf,size);
	if(s < 0){
		print_error(("Error: reading error"));
	}
	return s;
}
// Referenced my lab0 read_write() function
//"It is suggested that you should do a large (e.g., 256 byte) read, and 
// then process however many characters you actually receive."
void read_write(){
	char buf[256];
	ssize_t s = read(0,&buf,sizeof(char)*256);
	while(s > 0)
	{
		for(int i = 0; i < s ; i++)
		{
			char c = buf[i];
			if (c == CTRLD || c == 0x04){
				exit(0);
				break;
			}
			else if (c == CTRLC || c == 0x03){
			  write_with_check(1,&arrow_C,sizeof(char)*2);
			}
			else if (c == CR || c == LF || c == crlf[0] || c == crlf[1])
			{
				write_with_check(1,&crlf,sizeof(char)*2);
			}
			else{
				//write_with_check(1,&crlf, sizeof(char)*1);
				write_with_check(1,&c, sizeof(char)*1);
			}
		}
		s = read(0,&buf,sizeof(char)*256);
	}
	if (s < 0){
		print_error("Error: reading error during regular case (no --shell option)");
	}
}

/*-----------------------------[--shell]---------------------------------------*/
void shell_exit(){
	int status;
	waitpid(pid,&status,0);
	// https://stackoverflow.com/questions/3659616/why-does-wait-set-status-to-256-instead-of-the-1-exit-status-of-the-forked-pr
	//https://www.geeksforgeeks.org/exit-status-child-process-linux/
	//https://stackoverflow.com/questions/3659616/why-does-wait-set-status-to-256-instead-of-the-1-exit-status-of-the-forked-pr
	if (WIFEXITED(status)) {
		fprintf(stderr,"SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
		//exit(0); // dont need really
	}
	if (WIFSIGNALED(status)){
	    fprintf(stderr,"SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
	    //exit(0);
	}
} // Current version: if we do ctrl C ,program exits with code 0, printing with signal 2
// if we just what the WIFEXITED if statement it just returns0 without anything printed out
void make_pipe(int p[2]){
	if (pipe(p) < 0)
	{
		print_error("Error: creation of pipe(s)");
	}
}
void signal_handler(int sig){
	// THESE TWO LINES NEW:
	close(p_to_c[1]);
	close(c_to_p[0]);
	if(sig == SIGPIPE || sig == SIGINT){
		kill(pid, SIGINT);
		shell_exit();
		exit(0); //dont need really
	}
}
void check_dup2(int old, int new){
	if(dup2(old,new) < 0){
		print_error("Error: dup2 error during I/O redirection");
	}
}
void check_close(int fd){
	if (close(fd) < 0){
		print_error("Error: close error during I/O redireciton");
	}
}
/*-----child and parent case (linux command /bin/bash exec vs regular input)*/
void child_case(){
/*
whose standard input is a pipe from the terminal process, and whose standard output and 
standard error are (dups of) a pipe to the terminal process.
*/
	check_close(p_to_c[1]);
	check_close(c_to_p[0]);
	check_dup2(p_to_c[0], 0);
	check_dup2(c_to_p[1], 1);
	check_dup2(c_to_p[1], 2);
	check_close(p_to_c[0]);
	check_close(c_to_p[1]);
	char* pathname = "/bin/bash";
	char* args[2] = {pathname,NULL};
	if(execvp(pathname,args) == -1){
		print_error("Error: failed execute command");
	}
}

void parent_case() {
	check_close(p_to_c[0]);
	check_close(c_to_p[1]);
	short pevents =  POLLIN | POLLERR | POLLHUP;
	// https://man7.org/linux/man-pages/man2/poll.2.html
	struct pollfd fds[2] = {
	//    int   fd;         /* file descriptor */
	//   short events;     /* requested events */
	//   short revents;    /* returned events */
		{0, pevents, 0},
		{c_to_p[0], pevents, 0}
	};
	char buf[256];
	char c;
	while(1){
		int ret = poll(fds,2,0); // poll(fds,2,-1);
		if (ret < 0){
			print_error("Error: polling");
		}
		if (ret > 0){
			// returned events
			short stdin_e = fds[0].revents;
			short shell_e = fds[1].revents;
			// INPUT KEYBOARD
			if(stdin_e & POLLIN){
				//char buf[256];
				ssize_t s = read_with_check(0,&buf,sizeof(char)*256);
				for(int i = 0; i < s ; i++)
				{
					//char c = buf[i];
					c = buf[i];
					if (c == CTRLD || c == 0x04){
					//	if (shell_flag)
						//{
					   write_with_check(1,&arrow_D,sizeof(char)*2);

							check_close(p_to_c[1]);
							/*close the pipe to the shell, but continue processing input from the shell. 
							We do this because there may still be output in transit from the shell.
							Recall, read input from the shell pipe and write it to stdout. If it receives an <lf> from the shell, 
							it should print it to the screen as <cr><lf>*/
							//char temp_buf[256];
							ssize_t input_shell = read_with_check(c_to_p[0], &buf,sizeof(char)*256);
							for (int i = 0; i < input_shell; i++){
								if(c == LF || c == crlf[1])
									write_with_check(1,&crlf,sizeof(char)*2);
								else
									write_with_check(1,&c, sizeof(char)*1);
							}
							close(c_to_p[0]);
							exit(0);
					} // if CTRLD
					else if (c == CTRLC || c == 0x03){
					  write_with_check(1,&arrow_C,sizeof(char)*2);
						kill(pid, SIGINT);
					} // if CTRLD
					else if (c == CR || c == LF || c == crlf[0] || c == crlf[1]){
					// "<cr> or <lf> should echo as <cr><lf> but go to shell as <lf>.
						char lf = LF;
						write_with_check(1,&crlf,sizeof(char)*2);
						write_with_check(p_to_c[1],&lf,sizeof(char)*1);
					} // if CR or LF
					else{
						write_with_check(0,&c,sizeof(char)*1);
						write_with_check(p_to_c[1],&c,sizeof(char)*1);
					}

				} // for
			} // stdin_e && POLLIN
			/*-- SHELL INPUT--*/
			if (shell_e & POLLIN){
				ssize_t shell_s = read_with_check(c_to_p[0], &buf, sizeof(char)*256);
				for (int i = 0; i < shell_s; i++)
				{
					c = buf[i];
					if (c == LF){
						write_with_check(1,&crlf,sizeof(char)*2);
					} else {
						write_with_check(1, &c,sizeof(char)*1);
					}
				}
			}// if stdin_e && POLLIN
			if ((POLLHUP | POLLERR) & shell_e){
				exit(0);
			} // end POLLUP POLLERR
		} // if ret > 0
	} // while(1)
	
} // parent_case

// http://www.cs.loyola.edu/~jglenn/702/S2005/Examples/dup2.html
// https://github.com/raoulmillais/linux-system-programming/blob/master/src/poll-example.c
