// import all libraries
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h> 
#include <sys/wait.h> 
#include <errno.h>

// define constants
#define COMMAND_LENGTH 1024
#define NUM_TOKENS (COMMAND_LENGTH / 2 + 1)
#define PATH_MAX 1024
#define EV_COUNT 1024
// history command 
#define HISTORY_DEPTH 1000 

static int CMDnum = 0; 	// where to insert next command into history; intiallly 0, incremented with every addition
char history[HISTORY_DEPTH][COMMAND_LENGTH]; //Array of 1000 commands entered
pid_t pid_ar[HISTORY_DEPTH];
int pid_ctr = 0;
char env_name[HISTORY_DEPTH][COMMAND_LENGTH];
char env_value[HISTORY_DEPTH][COMMAND_LENGTH];
int env_add = 0;

// DECLARE 'input_buffer' and 'tokens' as global variables so we can use them during the signal handling for displaying help on pressing CTRL+C!!
char input_buffer[COMMAND_LENGTH]; 
char *tokens[NUM_TOKENS];

// HELPER FUNCTIONS
void handle_signal(int signal); // signal handler

void addcmd(char buff[]){ // ADD commmand to history - Problem 3
	strcpy(history[CMDnum],buff);
	CMDnum++;
}

void getCMD(int n){ // write() one command depending on the CMDnum value
	char buffI[6];
	//sprintf(buffI,"%d",n);
	//write(STDOUT_FILENO,buffI,strlen(buffI));
	//write(STDOUT_FILENO,"\t",strlen("\t"));
	write(STDOUT_FILENO,history[n],strlen(history[n]));
	write(STDOUT_FILENO,"\n",strlen("\n"));
}

void printcmd(){ //print the last 10 commmands
	int i = CMDnum - 2;
	if(i <= 4){
		while(i >= 0){
			getCMD(i);
			i--;
		}
	}
	else{
		int s = i;
		int e = s-4;
		while(s >= e){
			getCMD(s);
			s--;
		}
	}
}

/**
 * Command Input and Processing
 */

/*
 * Tokenize the string in 'buff' into 'tokens'.
 * buff: Character array containing string to tokenize.
 *       Will be modified: all whitespace replaced with '\0'
 * tokens: array of pointers of size at least COMMAND_LENGTH/2 + 1.
 *       Will be modified so tokens[i] points to the i'th token
 *       in the string buff. All returned tokens will be non-empty.
 *       NOTE: pointers in tokens[] will all point into buff!
 *       Ends with a null pointer.
 * returns: number of tokens.
 */

int tokenize_command(char *buff, char *tokens[])
{
	int token_count = 0;
	_Bool in_token = false;
	int env_tok = -1;
	int num_chars = strnlen(buff, COMMAND_LENGTH);
	for (int i = 0; i < num_chars; i++) {
		switch (buff[i]) {
		// Handle token delimiters (ends):
		case ' ':
		case '\t':
		case '\n':
			buff[i] = '\0';
			in_token = false;
			break;

		// Handle other characters (may be start)
		default:
			if (!in_token) {
				//tokens[token_count] = &buff[i];
				if(buff[i] == '$'){
					env_tok = token_count;
					tokens[token_count] = &buff[i+1];
				}
				else{
					tokens[token_count] = &buff[i];
				}
				token_count++;
				in_token = true;
			}
		}
	}
	if (env_tok >= 0){
		char out[40];
		sprintf(out, "%d", env_add);
		write(STDOUT_FILENO,out,strlen(out));

		char *enm = tokens[env_tok];
		for(int i = 0; i < env_add; i++){
			if(strcmp(enm, env_name[i]) == 0){
				tokens[env_tok] = &(env_value[i][0]);

			}
		}
	}
	tokens[token_count] = NULL;
	return token_count;
}

/**
 * Read a command from the keyboard into the buffer 'buff' and tokenize it
 * such that 'tokens[i]' points into 'buff' to the i'th token in the command.
 * buff: Buffer allocated by the calling code. Must be at least
 *       COMMAND_LENGTH bytes long.
 * tokens[]: Array of character pointers which point into 'buff'. Must be at
 *       least NUM_TOKENS long. Will strip out up to one final '&' token.
 *       tokens will be NULL terminated (a NULL pointer indicates end of tokens).
 * in_background: pointer to a boolean variable. Set to true if user entered
 *       an & as their last token; otherwise set to false.
 * if returns 0 --> donont read previous command, if 1 --> read and exec()
 */
int read_command(char *buff, char *tokens[], _Bool *in_background) // modified for '!!' and '!n' command
{
	*in_background = false;

	// Read input
	int length = read(STDIN_FILENO, buff, COMMAND_LENGTH-1);

	if ( (length <= 0) && (errno !=EINTR) ){
    	write(STDOUT_FILENO,"Unable to read command. Terminating.\n", strlen("Unable to read command. Terminating.\n"));
    	return -1;  /* terminate with error */
	}

	// Null terminate and strip \n.
	buff[length] = '\0';
	/*
	for(int i = 0; i < length ; i++){
		if (buff[i] == '='){
			*is_env = true;
			//buff[i] = ' ';
		}
	}
	*/
	if (buff[0] == '&'){
		*in_background = true;
		buff[0] = ' ';
	}
	if (buff[strlen(buff) - 1] == '\n') {
		buff[strlen(buff) - 1] = '\0';
	}

	if(strlen(buff)> 1 && buff[0] == '!' && buff[1] == '!'){ // '!!' command to be run - change buff to last command entered
		buff = history[CMDnum - 1];
		addcmd(buff);
		write(STDOUT_FILENO,buff,strlen(buff));
		write(STDOUT_FILENO,"\n",strlen("\n"));
	}
	else if(buff[0] == '!'){ // '!_' command to be run
		char cnum[10];
		int j = 0;
		for(int i = 1; buff[i] != '\0'; i++){
			if(buff[i] >= 48 && buff[i] <= 57){
				cnum[j] = buff[i];
				j++;
			}
		}
		cnum[j] = '\0';
		if(cnum[0] == '\0'){ // not a number entered
			write(STDOUT_FILENO,"\nERROR: invalid argument, try entering a number after '!'\n",strlen("\nERROR: invalid argument, try entering a number after '!'\n"));
		}
		else{ 
			int n = atoi(cnum);
			if(n < CMDnum - 11){ // number not in range
				write(STDOUT_FILENO,"\nERROR: inalid argument, try entering a number in range\n",strlen("\nERROR: inalid argument, try entering a number in range\n"));
			}
			else{ // correct argument entered
				buff = history[n];
				addcmd(buff);
				write(STDOUT_FILENO,buff,strlen(buff));
				write(STDOUT_FILENO,"\n",strlen("\n"));
			}
		}
	}
	else{// add command to history[1000][1024] - array of command strings
		addcmd(buff);
	}
	
	// EDIT END ------------------------------------------------------------------------
	
	
	// Tokenize (saving original command string)
	int token_count = tokenize_command(buff, tokens);
	if (token_count == 0) {
		return -1;
	}
	return 1;

	// Extract if running in background:
	/*
	if (token_count > 0 && strcmp(tokens[token_count - 1], "&") == 0) {
		*in_background = true;
		tokens[token_count - 1] = 0;
	}
	*/
}


// Function Declarations for builtin Shell commmands
int myCD(char **args);
int myPWD(char **args);
int myHELP(char **args);
int myHistory(char **args);
int myPShist(char **args);

char *builtinCMDstr[] = {"cd", "pwd", "ps_history","history", "!!", "!n", "exit"};
int (*builtinCMDfunc[]) (char **) = { &myCD, &myPWD , &myPShist, &myHistory};

// Built-In cd commands for shell
int myCD(char **args){
	if(args[1] == NULL){ // no directory provided
		write(STDOUT_FILENO,"\nEXPECTED ARGUMENT TO 'cd'\n",strlen("\nEXPECTED ARGUMENT TO 'cd'\n"));
	} else{ 
		if(chdir(args[1]) != 0){
			write(STDOUT_FILENO,"No such directory as '",strlen("No such directory as '"));
			write(STDOUT_FILENO,args[1],strlen(args[1]));
			write(STDOUT_FILENO,"'\n",strlen("'\n"));
		}
	}
	return 1;
}
void ps_update(){
	pid_t wpid;
	int status;
	for(int i=0;i<pid_ctr;i++){
		//printf("%d\n%d\n",i,processes_id[i]);
		wpid=waitpid(pid_ar[i],&status,WNOHANG);
		//printf("%d, %d\n",kill(processes_id[i],0),processes_id[i]);
	}
	return;
}

int myPShist(char **args){
	ps_update();
	for(int i = 0; i < pid_ctr; i++){
		//write(STDOUT_FILENO,pid_ar[i],strlen(pid_ar[i]));
		printf("%ld", (long) pid_ar[i]);
		//printf(pid_ar[i]);
		if (kill(pid_ar[i],0) == 0){
			printf(" RUNNING\n");
			//write(STDOUT_FILENO," RUNNING\n",strlen(" RUNNING\n"));
		}
		else{
			printf(" STOPPED\n");
			//write(STDOUT_FILENO," STOPPED\n",strlen(" STOPPED\n"));
		}
	}
	return 1;
}
// Built-In pwd commmand for shell
int myPWD(char **args){
	char cwd[PATH_MAX];
	if(args[1] != NULL){
		write(STDOUT_FILENO,"\nERROR: Did not expect argument '",strlen("\nERROR: Did not expect argument '"));
		write(STDOUT_FILENO,args[1],strlen(args[1]));
		write(STDOUT_FILENO,"' for commmand 'pwd'\n",strlen("' for commmand 'pwd'\n"));
	} else{
		if(getcwd(cwd, sizeof(cwd)) != NULL){
			write(STDOUT_FILENO,"Current working directory: ",strlen("Current working directory: "));
			write(STDOUT_FILENO,cwd,strlen(cwd));
			write(STDOUT_FILENO,"\n",strlen("\n"));
		}
		else{
			write(STDOUT_FILENO,"\nERROR: could not load current working directory\n",strlen("\nERROR: could not load current working directory\n"));
		}
	}
	return 1;
}

// checking if commmand is builtin or external
int isBuiltin(char *str){
	int i = 0;
	while(i < 7){
		if(strcmp(str,builtinCMDstr[i]) == 0){
			return i;
		}
		i++;
	}
	return -1;
}



// Built-In help command for shell
int myHELP(char **args){
	if(args[2] != NULL){ // more than 1 argument passed with help
		write(STDOUT_FILENO,"\nERROR: Too many arguments for 'help'\n",strlen("\nERROR: Too many arguments for 'help'\n"));
		return 1;
	}
	if(args[1] == NULL){ // no argument passed with help
		write(STDOUT_FILENO,"******  HELP  ******\n",strlen("******  HELP  ******\n"));
		write(STDOUT_FILENO,"Type 'help' followed by command name and press ENTER!\n",strlen("Type 'help' followed by command name and press ENTER!\n"));
		write(STDOUT_FILENO,"The following are BUILT-IN commmands: \n",strlen("The following are BUILT-IN commmands: \n"));
		int i = 0;
		while(i < 7){
			write(STDOUT_FILENO," - ",strlen(" - "));
			write(STDOUT_FILENO,builtinCMDstr[i],strlen(builtinCMDstr[i]));
			if(strcmp(builtinCMDstr[i],"cd") == 0){
				write(STDOUT_FILENO," : built-in commmand for changing the current working directory\n\n",strlen(" : built-in commmand for changing the current working directory\n\n"));
			} else if(strcmp(builtinCMDstr[i],"pwd") == 0){
				write(STDOUT_FILENO," : built-in commmand for printing the current working directory\n\n",strlen(" : built-in commmand for printing the current working directory\n\n"));
			} else if(strcmp(builtinCMDstr[i],"exit") == 0){
				write(STDOUT_FILENO," : built-in commmand for exiting the program\n\n",strlen(" : built-in commmand for exiting the program\n\n"));
			} else if(strcmp(builtinCMDstr[i],"help") == 0){
				write(STDOUT_FILENO," : built-in commmand for displaying information on all commands[no arguments] or a specific command[with argument]\n\n",strlen(" : built-in commmand for displaying information on all commands[no arguments] or a specific command[with argument]\n\n"));
			} else if(strcmp(builtinCMDstr[i],"exit") == 0){
				write(STDOUT_FILENO," : built-in commmand for exiting the shell\n\n",strlen(" : built-in commmand for exiting the shell\n\n"));
			} else if(strcmp(builtinCMDstr[i],"history") == 0){
				write(STDOUT_FILENO," : built-in commmand for displaying last 10 commands\n\n",strlen(" : built-in commmand for displaying last 10 commands\n\n"));
			} else if(strcmp(builtinCMDstr[i],"!!") == 0){
				write(STDOUT_FILENO," : built-in commmand for executing the last command\n\n",strlen(" : built-in commmand for executing the last command\n\n"));
			} else if(strcmp(builtinCMDstr[i],"!n") == 0){
				write(STDOUT_FILENO," : built-in commmand for executing the nth-last command\n\n",strlen(" : built-in commmand for executing the nth-last command\n\n"));
			}
			i++;
		}
		return 1;
	} else{ // only one argument passed with help
		int bI = isBuiltin(args[1]);
		if(bI == 0){
			write(STDOUT_FILENO,"	- '",strlen("	- '"));
			write(STDOUT_FILENO,args[1],strlen(args[1]));
			write(STDOUT_FILENO,"' is a built-in command for changing the current working directory\n\n",strlen("' is a built-in command for changing the current working directory\n\n"));
			//return 1;
		} else if(bI == 1){
			write(STDOUT_FILENO,"	- '",strlen("	- '"));
			write(STDOUT_FILENO,args[1],strlen(args[1]));
			write(STDOUT_FILENO,"' is a built-in command for printing the current working directory\n\n",strlen("' is a built-in command for printing the current working directory\n\n"));
			//return 1;
		} else if(bI == 2){
			write(STDOUT_FILENO,"	- '",strlen("	- '"));
			write(STDOUT_FILENO,args[1],strlen(args[1]));
			write(STDOUT_FILENO,"' is a built-in command for showing info. on commands\n\n",strlen("' is a built-in command for showing info. on commands\n\n"));
		} else if(bI == 3){
			write(STDOUT_FILENO,"	- '",strlen("	- '"));
			write(STDOUT_FILENO,args[1],strlen(args[1]));
			write(STDOUT_FILENO,"' is a built-in command for exiting this shell\n\n",strlen("' is a built-in command for exiting this shell\n\n"));
		} else if(bI == 4){
			write(STDOUT_FILENO,"	- '",strlen("	- '"));
			write(STDOUT_FILENO,args[1],strlen(args[1]));
			write(STDOUT_FILENO,"' is a built-in command for displaying the last 10 executed commands\n\n",strlen("' is a built-in command for displaying the last 10 executed commands\n\n"));
		} else if(bI == -1){
			write(STDOUT_FILENO,"	- '",strlen("	- '"));
			write(STDOUT_FILENO,args[1],strlen(args[1]));
			write(STDOUT_FILENO,"' is an external commmand or application. Run 'man ",strlen("' is an external commmand or application. Run 'man "));
			write(STDOUT_FILENO,args[1],strlen(args[1]));
			write(STDOUT_FILENO,"' for more information\n\n",strlen("' for more information\n\n"));
		} else if(bI == 5){
			write(STDOUT_FILENO,"	- '",strlen("	- '"));
			write(STDOUT_FILENO,args[1],strlen(args[1]));
			write(STDOUT_FILENO,"' is a built-in command for runnning the last command entered\n\n",strlen("' is a built-in command for runnning the last command entered\n\n"));
		} else if(bI == 6){
			write(STDOUT_FILENO,"	- '",strlen("	- '"));
			write(STDOUT_FILENO,args[1],strlen(args[1]));
			write(STDOUT_FILENO,"' is a built-in command for runnning the nth-last command entered\n\n",strlen("' is a built-in command for runnning the nth-last command entered\n\n"));
		}
		return 1;
	}
	
}

// History command function
int myHistory(char **args){
	printcmd();
	return 1;
}


// Function for executing builtin commands
int shellCheck(char **args){
	int i = 0;
	if(args[0] == NULL){
		// Empty command
		return 1;
	}
	while(i < 5){
		if(strcmp(args[0],builtinCMDstr[i]) == 0){
			return 5;
		}else{
			i++;
		}
	}
	int size = sizeof args[0] / sizeof (args[0])[0];
	for (int x = 0; x < size ; x++){
		if ((args[0])[x] == '='){
			return 6;
		}
	}
	/*
	if(args[0][0] == '$'){
		return 5;
	}
	*/
	return 0;
}

int shellExec(char **args){
	int i = 0;
	if(args[0] == NULL){
		// Empty command
		return 1;
	}
	while(i < 5){
		if(strcmp(args[0],builtinCMDstr[i]) == 0){
			return (*builtinCMDfunc[i])(args);
		}else{
			i++;
		}
	}
	int size = sizeof args[0] / sizeof (args[0])[0];
	int split = -1;
	for (int i = 0; i < size ; i++){
		if ((args[0])[i] == '='){
			split = i;
		}
	}
	if (split >= 0){
		printf("HI");
		for(int i = 0; i < split; i++){
			env_name[env_add][i] = args[0][i];
		}
		env_name[env_add][split] = '\0';
		for(int i = split + 1; i < size; i++){
			env_value[env_add][i - split - 1] = args[0][i];
		}
		env_value[env_add][size-split-1] = '\0';
		env_add++;
		return 5;
	}
	
	/*
	if(args[0][0] == '$'){
		char *enm = &(args[0][1]);
		for(int i = 0; i < env_add; i++){
			if(strcmp(enm, env_name[i]) == 0){

			}
		}
	}
	*/
	return 0;
}

//signal handler
void handle_signal(int sig){
	signal(SIGINT, handle_signal);
	strcpy(input_buffer,"exit");
}

/**
 * Main and Execute Commands
 */
int main(int argc, char* argv[])
{
	//signal handling using sigaction() 
	struct sigaction sa;
	sa.sa_handler = &handle_signal;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	
	// shell loop
	while (true) {

		char cwd[PATH_MAX+1]; // current working directory - for prompt
		//write(STDOUT_FILENO," ~",strlen(" ~")); not required here
		if(getcwd(cwd, sizeof(cwd)) != NULL){
			write(STDOUT_FILENO,cwd,strlen(cwd));
		}
		write(STDOUT_FILENO, "~$ ", strlen("~$ "));
		_Bool is_piped = false;
		int pipe_break = -1;
		//_Bool is_env = false;
		//int env_break = -1;
		_Bool in_background = false;
		int validc = read_command(input_buffer, tokens, &in_background);
		if (validc == -1){
			continue;
		}
		// DEBUG: Dump out arguments:
		for (int i = 0; tokens[i] != NULL; i++) {
			if (strcmp(tokens[i], "|") == 0){
				is_piped = true;
				pipe_break = i;
			}
			write(STDOUT_FILENO, "   Token: ", strlen("   Token: "));
			write(STDOUT_FILENO, tokens[i], strlen(tokens[i]));
			write(STDOUT_FILENO, "\n", strlen("\n"));
		}
		write(STDOUT_FILENO,"\n",strlen("\n"));		
		// Built-In exit commmand for shell
		if (is_piped){
			char *token1[NUM_TOKENS];
			char *token2[NUM_TOKENS];
			for (int x=0; x< pipe_break; x++) {    
            	token1[x]=tokens[x];
            }     
            int z = 0;     
			int size = sizeof tokens / sizeof tokens[0];
            for (int x= pipe_break+1; x< size; x++) {     
                token2[z]=tokens[x];
                z++;
            }
			int fds[2];
			pipe(fds);
			//int i;
			pid_t pid = fork();
			if (pid == -1) { //error
				char *error = strerror(errno);
				printf("error fork!!\n");
				exit(-1);
			} 
			if (pid > 0) { // parent process
				waitpid(pid, NULL,0);
				pid_ar[pid_ctr] = pid;
				pid_ctr++;
				int save_in;
				save_in = dup(STDIN_FILENO);
				close(fds[1]);
				dup2(fds[0], STDIN_FILENO);
				//close(fds[0]);
				pid_t pid2 = fork();
				if (pid2 < 0){
					char *error = strerror(errno);
					printf("error fork!!\n");
					exit(-1);
				}
				else if (pid2 == 0){
					if (shellCheck(token2) == 0){
						execvp(token2[0], token2); // run command BEFORE pipe character in userinput
						char *error = strerror(errno);
						printf("unknown command\n");
						return 0;
					}
					else if (shellCheck(token2) == 5){
						shellExec(token2);
						exit(1);
					}
				}
				else{
					waitpid(pid2, NULL, 0);
					pid_ar[pid_ctr] = pid2;
					pid_ctr++;
					dup2(save_in, STDIN_FILENO);
					//write(STDOUT_FILENO,"Command Executed\n",strlen("Command Executed\n"));
				}
			} else { // child process
				//wait(NULL);
				close(fds[0]);
				dup2(fds[1], STDOUT_FILENO);
				if (shellCheck(token1) == 0){
					execvp(token1[0], token1); // run command AFTER pipe character in userinput
					char *error = strerror(errno);
					printf("unknown command\n");
					return 0;
				}
				else if (shellCheck(token1) == 5){
					shellExec(token1);
					exit(1);
				}
			}

		}
		else{

		
		if(strcmp(tokens[0],"exit") == 0) // if "exit" commmand entered then exit shell
		{
			write(STDOUT_FILENO,"Exiting shell ...\n.",strlen("Exiting shell ....\n"));
			return 0;
		} else{ // check if inbuilt commmand
			if(shellCheck(tokens) == 0) // if not inbuilt command, fork and call external functions for command
			{
				pid_t pid = fork();
				if(pid == -1){ // Error in forking
					write(STDOUT_FILENO," ERROR FORKING\n", strlen(" ERROR FORKING\n"));
				continue;
				} else if(pid == 0){ // Child Processing
				if (in_background){
					write(STDOUT_FILENO,"\n",strlen("\n"));
					write(STDOUT_FILENO,"Command Executed: after runnning in background\n",strlen("Command Executed: after runnning in background\n"));
					execvp(tokens[0],tokens);
				}
				if (!in_background){
					execvp(tokens[0],tokens);
				}
				
					// Reaches here only if exec() is not able to run
					write(STDOUT_FILENO,"COULD NOT RUN COMMAND '",strlen("COULD NOT RUN COMMAND '"));
					write(STDOUT_FILENO,tokens[0],strlen(tokens[0]));
					write(STDOUT_FILENO,"'\n",strlen("'\n"));
					exit(-1);
				} else{ // Parent Process
					if (in_background) {
					write(STDOUT_FILENO, "Run in background.\n", strlen("Run in background.\n"));
					pid_ar[pid_ctr] = pid;
					pid_ctr++;
					// wait(NULL);
					//write(STDOUT_FILENO,"Command Executed: after runnning in background\n",strlen("Command Executed: after runnning in background\n"));
					} 
					else{
						pid_ar[pid_ctr] = pid;
						pid_ctr++;
						waitpid(pid,NULL,0);
						write(STDOUT_FILENO,"Command Executed\n",strlen("Command Executed\n"));
					}
				}
				
			}
			else if(shellCheck(tokens) == 5){
				pid_t pid = fork();
				if(pid == -1){ // Error in forking
					write(STDOUT_FILENO," ERROR FORKING\n", strlen(" ERROR FORKING\n"));
				continue;
				} else if(pid == 0){ // Child Processing
				if (in_background){
					write(STDOUT_FILENO,"\n",strlen("\n"));
					write(STDOUT_FILENO,"Command Executed: after runnning in background\n",strlen("Command Executed: after runnning in background\n"));
					shellExec(tokens);
				}
				if (!in_background){
					shellExec(tokens);
				}
					exit(1);
					// Reaches here only if exec() is not able to run
					write(STDOUT_FILENO,"COULD NOT RUN COMMAND '",strlen("COULD NOT RUN COMMAND '"));
					write(STDOUT_FILENO,tokens[0],strlen(tokens[0]));
					write(STDOUT_FILENO,"'\n",strlen("'\n"));
					exit(-1);
				} else{ // Parent Process
					if (in_background) {
					write(STDOUT_FILENO, "Run in background.\n", strlen("Run in background.\n"));
					pid_ar[pid_ctr] = pid;
					pid_ctr++;
					// wait(NULL);
					//write(STDOUT_FILENO,"Command Executed: after runnning in background\n",strlen("Command Executed: after runnning in background\n"));
					} 
					else{
						pid_ar[pid_ctr] = pid;
						pid_ctr++;
						waitpid(pid,NULL,0);
						write(STDOUT_FILENO,"Command Executed\n",strlen("Command Executed\n"));
					}
				}
			}
			else if (shellCheck(tokens) == 6){
				shellExec(tokens);
			}
		}
	}
	}
}