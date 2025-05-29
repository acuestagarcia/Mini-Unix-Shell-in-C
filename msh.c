//  MSH main file

//#include "parser.h"
#include <stddef.h>			/* NULL */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_COMMANDS 8

void write_error_to_stdout(char * output){
    strcpy(output, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
    if (write(STDOUT_FILENO, output, strlen(output)) < strlen(output)) {
        perror("Failed to write an error of mycalc to stdout");
        exit(-1);
    }
}

void write_correct_output_to_stderr(char * output){
    // write the correct output to the standard error
    if (write(STDERR_FILENO, output, strlen(output)) < strlen(output)) {
        perror("Failed to write a correct output of mycalc to stderr");
    }
}

// files in case of redirection
char filev[3][64];

//to store the execvp second parameter
char *argv_execvp[8];

void siginthandler(int param)
{
	printf("****  Exiting MSH **** \n");
	//signal(SIGINT, siginthandler);
	exit(0);
}

/* myhistory */

struct command
{
  // Store the number of commands in argvv
  int num_commands;
  // Store the number of arguments of each command
  int *args;
  // Store the commands
  char ***argvv;
  // Store the I/O redirection
  char filev[3][64];
  // Store if the command is executed in background or foreground
  int in_background;
};

int history_size = 20;
struct command * history;
int head = 0;
int tail = 0;
int n_elem = 0;

void free_command(struct command *cmd)
{
    if((*cmd).argvv != NULL)
    {
        char **argv;
        for (; (*cmd).argvv && *(*cmd).argvv; (*cmd).argvv++)
        {
            for (argv = *(*cmd).argvv; argv && *argv; argv++)
            {
                if(*argv){
                    free(*argv);
                    *argv = NULL;
                }
            }
        }
    }
    free((*cmd).args);
}

void store_command(char ***argvv, char filev[3][64], int in_background, struct command* cmd)
{
    // Count the number of commands in the command sequence
    int num_commands = 0;
    while(argvv[num_commands] != NULL){
        num_commands++;
    }
    
    // Copy file redirection information to the command structure
    for(int f=0;f < 3; f++)
    {
        if(strcmp(filev[f], "0") != 0)
        {
            strcpy((*cmd).filev[f], filev[f]);
        }
        else{
            strcpy((*cmd).filev[f], "0");
        }
    }

    // Store background execution flag and actual number of commands
    (*cmd).in_background = in_background;
    (*cmd).num_commands = num_commands-1;
    
    // Allocate memory for command array and argument count array
    (*cmd).argvv = (char ***) calloc((num_commands) ,sizeof(char **));
    (*cmd).args = (int*) calloc(num_commands , sizeof(int));

    // Process each command in the sequence
    for( int i = 0; i < num_commands; i++)
    {
        // Count arguments for current command
        int args= 0;
        while( argvv[i][args] != NULL ){
            args++;
        }
        
        // Store argument count and allocate memory for arguments
        (*cmd).args[i] = args;
        (*cmd).argvv[i] = (char **) calloc((args+1) ,sizeof(char *));
        
        // Copy each argument string
        int j;
        for (j=0; j<args; j++)
        {
            (*cmd).argvv[i][j] = (char *)calloc(strlen(argvv[i][j]),sizeof(char));
            strcpy((*cmd).argvv[i][j], argvv[i][j] );
        }
    }
}


/**
 * Get the command with its parameters for execvp
 * Execute this instruction before run an execvp to obtain the complete command
 * @param argvv
 * @param num_command
 * @return
 */
void getCompleteCommand(char*** argvv, int num_command) {
	//reset first
	for(int j = 0; j < 8; j++)
		argv_execvp[j] = NULL;

	int i = 0;
	for ( i = 0; argvv[num_command][i] != NULL; i++)
		argv_execvp[i] = argvv[num_command][i];
}


/**
 * Main shell  Loop  
 */
int main(int argc, char* argv[]){
	int end = 0; 
	int executed_cmd_lines = -1;
	char *cmd_line = NULL;
	char *cmd_lines[10];

	if (!isatty(STDIN_FILENO)) {
		cmd_line = (char*)malloc(100);
		while (scanf(" %[^\n]", cmd_line) != EOF){
			if(strlen(cmd_line) <= 0) return 0;
			cmd_lines[end] = (char*)malloc(strlen(cmd_line)+1);
			strcpy(cmd_lines[end], cmd_line);
			end++;
			fflush (stdin);
			fflush(stdout);
		}
	}

	/*********************************/

	char ***argvv = NULL;
	int num_commands;

	history = (struct command*) malloc(history_size *sizeof(struct command));
	int run_history = 0;

    int command_counter = 0;
	int in_background = 0;
    // this one has been added
    int history_index = 0;

	while (1) {

		int status = 0;
		//int command_counter = 0;
		//int in_background = 0;
		signal(SIGINT, siginthandler);

		if (run_history) {
            /*  in case myhistory <index> was run on the last iteration, then we need to manually free and update argvv, filev and in_background
                with the ones stored in the struct command of the command with the chosen index*/
            
            // free the memory currently allocated to argvv and its sub-elements

            for (int i = 0; i < 2; i++) {
                // free the memory used by the strings ("myhistory" and "<index>")
                free(argvv[0][i]);
            }
            // free the memory used by the array holding the command
            free(argvv[0]);
            // free the memoery used by the array holding the command sequence
            free(argvv);

            // allocate memory to argvv to hold num_commands pointers
            if ((argvv = malloc(history[history_index].num_commands * sizeof(char **))) == NULL){
                perror("Error in `malloc()` for argvv");
                exit(-1);
            }

            for (int i = 0; i < history[history_index].num_commands + 1; i++){
                // for each command, allocate memory to hold args[<command index>] pointers
                if ((argvv[i] = malloc(history[history_index].args[i] * sizeof(char *))) == NULL){
                    perror("Error in `malloc()` for argvv");
                    exit(-1);
                }

                for (int j = 0; j < (history[history_index].args[i]); j++){
                    // for each argument, allocate memory to hold the length of its string + 1 (null terminator)
                    if ((argvv[i][j] = malloc((strlen(history[history_index].argvv[i][j]) + 1) * sizeof(char))) == NULL){
                        perror("Error in `malloc()` for argvv");
                        exit(-1);
                    }

                    // copy each argument as a string
                    strcpy(argvv[i][j], history[history_index].argvv[i][j]);
                }
            }

            for (int i=0; i < 3; i++){
                // copy new redirection files
                strcpy(filev[i], history[history_index].filev[i]);
            }

            // update in_background
            in_background = history[history_index].in_background;

            // update command_counter
            command_counter = history[history_index].num_commands;

            // restart the value of run_history
            run_history=0; 
        }
        else{
            // Prompt 
            write(STDERR_FILENO, "MSH>>", strlen("MSH>>"));
            // Get command
            executed_cmd_lines++;
            if( end != 0 && executed_cmd_lines < end) {
                command_counter = read_command_correction(&argvv, filev, &in_background, cmd_lines[executed_cmd_lines]); // CORRECTION MODE
            }
            else if( end != 0 && executed_cmd_lines == end)
                return 0;
            else
                command_counter = read_command(&argvv, filev, &in_background); //NORMAL MODE
        }
	//************************************************************************************************

        if (command_counter > 0) {
            if (command_counter > MAX_COMMANDS){
                printf("Error: Maximum number of commands is %d \n", MAX_COMMANDS);
            }
            // in the following block we execute the commands that are not internal
            else if ((strcmp(argvv[0][0], "mycalc") != 0) && (strcmp(argvv[0][0], "myhistory") != 0)){

                // store the command in the last position (tail) of the struct command
                store_command(argvv, filev, in_background, &history[tail]);
                /* update the tail (the % ensures that the tail index wraps around to the start of the 
                struct when it reaches the end) */
                tail = (tail + 1) % history_size;
                // in case the struct command is not full yet (20 commands)
                if (n_elem < history_size){
                    // add 1 to the counter
                    n_elem++;
                }
                else{
                    // if it is full, we need to update the head to the second oldest command
                    head = (head + 1) % history_size;
                    // erase the oldest command to make room for the new command
                    free_command(&history[head]);
                }

                // the parent process will need to know the pid of its children
                int pid;
                int child_pids[command_counter];

                /* in order to implement a command sequence, two pipes may need to be used simultaneously 
                for those commands that get their inputs from another processes and also write their outputs to another processes  */ 
                // this pair of file descriptors will hold the pipe to get inputs from the previous command
                // it is initialized to impossible file descriptors (-1) because the first process won't have to use it
                int old_fd[2] = {-1, -1};
                // this pair will hold the pipe to pass outputs to the next command
                int new_fd[2];

                // we need to create command_counter processes
                for (int i = 0; i < command_counter; i++){
                    // a pipe needs to be created for every command of the command sequence (except the last one)
                    if (i != (command_counter - 1)){ 
                        if (pipe(new_fd) == -1){
                            perror("Error with `pipe()`");
                            exit(-1);
                        }
                    }

                    // create a child process that is a clone of the parent
                    pid = fork(); 
                    switch (pid){
                        case -1:
                            perror ("Error with `fork()`");
                            exit(-1);

                        //child goes here
                        case 0:  
                            // if the user requested a redirection of the error output, we need to redirect it for every child process
                            if(filev[2][0] != '0'){
                                close(STDERR_FILENO);
                                if (open(filev[2], O_WRONLY | O_CREAT | O_TRUNC) == -1) {
                                    perror("Error opening the redirected error file in child");
                                    exit(-1);
                                }
                            }


                            /* if we are not in the first command, we need to redirect the input from the previous command 
                                by using the file descriptors of the old pipe*/
                            if (old_fd[0] != -1){
                                close(STDIN_FILENO);
                                if (dup(old_fd[0]) == -1) {
                                    perror("Error with `dup()`");
                                    exit(-1);
                                }
                                // we close the old file descriptors as we don't need them anymore
                                close(old_fd[0]);
                                close(old_fd[1]);
                            }
                            // for the first command of the sequence, this handles the user-requested redirection of the input
                            else if(filev[0][0] != '0'){
                                close(STDIN_FILENO);
                                if (open(filev[0], O_RDONLY) == -1) {
                                    perror("Error opening the redirected input file in child");
                                    exit(-1);
                                }
                            }


                            /* if we are not in the last command, we need to redirect the output to the next command
                                by using the file descriptors of the new pipe*/ 
                            if (i != (command_counter - 1)){
                                close(STDOUT_FILENO);
                                if (dup(new_fd[1]) == -1) {
                                    perror("Error with `dup()`");
                                    exit(-1);
                                }
                                // we close the new file descriptors as we don't need them anymore
                                close(new_fd[0]);
                                close(new_fd[1]);
                            }
                            // for the last command of the sequence, this handles the user-requested redirection of the output
                            else if(filev[1][0] != '0'){
                                close(STDOUT_FILENO);
                                if (open(filev[1], O_WRONLY | O_CREAT | O_TRUNC) == -1) {
                                    perror("Error opening the redirected output file in child");
                                    exit(-1);
                                }
                            }

                            // execute the command stored in the i-th position
                            execvp(argvv[i][0], argvv[i]);
                            // if execvp returns, then there has been an error
                            perror("Error in child's execvp");
                            exit(-1);

                        // parent goes here
                        default: 
                            /* in case this is not the iteration of the creation of the first child,
                                the parent can close the file descriptors of the old pipe because they
                                have already fulfilled their purpose (in the child)*/
                            if (old_fd[0] != -1) {
                                close(old_fd[0]);
                                close(old_fd[1]);
                            }
                            // the new file descriptors turn into the old ones for the next command
                            old_fd[0] = new_fd[0];
                            old_fd[1] = new_fd[1];

                            /*  store in an array the pid of each child command to be run in the foreground,
                                this is done so that, later, the parent knows which children require waiting */ 
                            if (!in_background){
                                child_pids[i] = pid;
                            }
                            
                    }
                }

                // if the command is executed in background, the minishell must execute the command without waiting
                if (in_background){
                    // print the pid of the last command   
                    printf("%d\n", pid);
                }
                else{
                    // wait for child processes to prevent them from becoming zombies
                    for (int i = 0; i < command_counter; i++) {
                        int status;
                        waitpid(child_pids[i], &status, 0);
                    }
                    /*  After executing a command in foreground, the minishell can not have 
                        zombie processes of previous commands executed in background. */
                    /*  To do so, we call waitpid with -1 to wait for the terminated background child processes
                        The WNOHANG option means that this call will not block the parent
                        If there are no more child processes that have terminated at the moment 
                        Waitpid will return 0 when there are no more terminated child processes left */
                    while (waitpid(-1, NULL, WNOHANG) > 0);
                }        
            }


            // execute internal command mycalc
            else if (strcmp(argvv[0][0], "mycalc") == 0){
                // output will store the string to be printed to the user
                // 200 characters is an arbitrary number that should be enough to store operations with big numbers
                char output[200];
                /* first, we should check that we have received all the expected three arguments,
                in case the user gave less, then one of the following elements would be NULL
                (because that is the way argvv was created) */
                if (argvv[0][1] != NULL && argvv[0][2] != NULL && argvv[0][3] != NULL) {
                    //convert operand to int, so that we can operate with it
                    int operand1 = atoi(argvv[0][1]);
                    int operand2 = atoi(argvv[0][3]);
                    /* atoi returns 0 both when given "0" (correct input that the user may give) 
                        and any string that doesn't have a valid integer representation*/
                    if ( (((operand1 == 0) && (strcmp(argvv[0][1], "0"))) != 0) || (((operand2 == 0) && (strcmp(argvv[0][2], "0"))) != 0)) {
                        // print error when argv[0][1] contains a non-valid integer representation
                        write_error_to_stdout(output);
                    } 
                    else{
                        // Check which is the operation to perform
                        if (strcmp(argvv[0][2], "add") == 0) {
                            
                            int acc_curr_val;
                            // first, we need to get the current value (a string) of Acc
                            char * acc_str = getenv("Acc");
                            if (acc_str){
                                acc_curr_val = atoi(acc_str);
                            }
                            // getenv() returns null if Acc doesn't yet exist (this is the first time add is executed)
                            else{  
                                //initialize Acc to 0
                                acc_curr_val = 0;
                            }

                            // compute new value of Acc
                            acc_curr_val += (operand1 + operand2);

                            // the value of the environmental variable must be stored as a string 
                            // 20 is an arbitrary number (it should be enough to store big numbers)
                            char new_acc_value[20]; 
                            snprintf(new_acc_value, sizeof(new_acc_value), "%d", acc_curr_val);

                            /* update the environment variable with the new sum,
                                the last argument (1) indicates that if there is an existing Acc,
                                it should be overwritten by this one that carries the updated value */
                            if (setenv("Acc", new_acc_value, 1) == -1){
                                perror("Error in `setenv()`");
                                exit(-1);
                            }

                            // store the correct output message as a string in the variable output
                            snprintf(output, sizeof(output), "[OK] %d + %d = %d; Acc %s\n", operand1, operand2, operand1 + operand2, getenv("Acc"));
                            write_correct_output_to_stderr(output);
                        } 
                        else if (strcmp(argvv[0][2], "mul") == 0) {
                            // store the correct output message as a string in the variable output
                            snprintf(output, sizeof(output), "[OK] %d * %d = %d\n", operand1, operand2, operand1 * operand2);
                            write_correct_output_to_stderr(output);
                        } 
                        else if (strcmp(argvv[0][2], "div") == 0) {
                            // first check if operand2 is not zero
                            if (operand2 == 0) {
                                write_error_to_stdout(output);
                            }
                            else{
                                // store the correct output message as a string in the variable output
                                snprintf(output, sizeof(output), "[OK] %d / %d = %d; Remainder %d\n", operand1, operand2, operand1 / operand2, operand1%operand2);
                                write_correct_output_to_stderr(output);
                            }
                        } 
                        else {
                            // operator not recognized
                            write_error_to_stdout(output);
                        }
                        
                    }
                } 
                else {
                    // not enough arguments provided
                    write_error_to_stdout(output);
                }
            }

            // execute internal command myhistory
            else if ((strcmp(argvv[0][0], "myhistory") == 0)){
                char output[200];
                // first, check if an argument has been provided
                if (argvv[0][1] != NULL){
                    // convert char into int
                    history_index = atoi(argvv[0][1]);
                    if (history_index == 0 && strcmp(argvv[0][1], "0") != 0) {
                        // print error when argv[0][1] ncontains a non-valid integer representation
                        snprintf(output, sizeof(output), "ERROR: Command not found\n");
                        if (write(STDOUT_FILENO, output, strlen(output)) < strlen(output)) {
                            perror("Failed to write an error of myhistory to stdout");
                            exit(-1);
                        }
                    } 
                    // check if the index is in a valid range (between 0 and the number of commands)
                    else if((0 <= history_index) && (history_index < (n_elem))){
                        /*  indicates whether the next command to be executed comes from the internal command myhistory
                            in this way, the code will avoid executing read_command() */
                        run_history = 1;
                        // copy the message to be printed into output
                        snprintf(output, sizeof(output), "Running command %s\n", argvv[0][1]);
                        // write this correct output to the standard error
                        if (write(STDERR_FILENO, output, strlen(output)) < strlen(output)) {
                            perror("Failed to write a correct output of myhistory to stderr");
                            exit(-1);
                        }

                        /*  storing each part of the selected command into the provided variables 
                            (argvv, filev, in_background, command_counter) has been added above (in the first if inside the while(1)) */
                        
                    }
                    else{
                        //print error when number given is not in the valid range
                        snprintf(output, sizeof(output), "ERROR: Command not found\n");
                        if (write(STDOUT_FILENO, output, strlen(output)) < strlen(output)) {
                            perror("Failed to write an error of myhistory to stdout");
                            exit(-1);
                        }
                    }
                    // erase output
                    output[0] = '\0';       
                }
                else{
                    // i is the index used to access each elem of the struct command
                    for (int i = 0; i < n_elem; i++){
                        // we need to add the index of each command in the array of struct commands
                        char command_index[4];
                        snprintf(command_index, sizeof(command_index), "%d ", i);
                        strcat(output, command_index);
                        // j is the index used to access each command inside the elem
                        for (int j = 0; j < history[i].num_commands; j++){
                            // k is the index used to access the arguments of each command
                            for (int k = 0; k < (history[i].args[j]); k++){
                                // concatenate to output each argument of each command of the command sequence
                                strcat(output, history[i].argvv[j][k]);
                                // add " " between the arguments of a command
                                if (k != (history[i].args[j] - 1)){
                                    strcat(output, " ");
                                }
                            }
                            // add " | " between the commands of a command sequence
                            if (j != (history[i].num_commands - 1)){
                                strcat(output, " | ");
                            }
                        }
                        // once we have stored the commands and their arguments, we need to store (if any) the file redirections
                        // this variable will later be concatenated to output
                        char file_redirection[100];
                        if (history[i].filev[0][0] != '0'){
                            snprintf(file_redirection, sizeof(file_redirection), " < %s", history[i].filev[0]);
                            strcat(output, file_redirection);
                            // erase contents from file_redirection for next iteration
                            file_redirection[0] = '\0';
                        }
                        if (history[i].filev[1][0] != '0'){
                            snprintf(file_redirection, sizeof(file_redirection), " > %s", history[i].filev[1]);
                            strcat(output, file_redirection);
                            // erase contents from file_redirection for next iteration
                            file_redirection[0] = '\0';
                        }
                        if (history[i].filev[2][0] != '0'){
                            snprintf(file_redirection, sizeof(file_redirection), " !> %s", history[i].filev[2]);
                            strcat(output, file_redirection);
                            // erase contents from file_redirection for next iteration
                            file_redirection[0] = '\0';
                        }
                        
                        // lastly, add the '&' in case the command was executed in the background
                        if (history[i].in_background){
                            strcat(output, " &");
                        }
                        // add a new line each time a command is printed
                        strcat(output, "\n");
                        // write this correct output to the standard error
                        if (write(STDERR_FILENO, output, strlen(output)) < strlen(output)) {
                            perror("Failed to write a correct output of myhistory to stderr");
                            exit(-1);
                        }
                        // erase the command from output
                        output[0] = '\0';
                    }
                }
            }
        }
        else if (command_counter == 0){ 
            exit(0);
        }
        else{
            perror("Error with `read_command()`");
        }    
    }
}


