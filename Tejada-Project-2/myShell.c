#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 
#include <ctype.h> 
#include <unistd.h>
#include <dirent.h> 
#include <sys/types.h>
#include <sys/wait.h> 
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <readline/readline.h>
#include <readline/history.h>

#define FALSE 0
#define TRUE 1
#define MAX_ARGS 20
#define BUFF 2048
#define color "\x1b[1m\x1b[32m" //https://gist.github.com/jampajeen/d917a4556487b0e2ea04e2bc848a43a0

void get_user_input(char *input, char *args[MAX_ARGS]);
int check_textf(char *input);
void run_textf(char *input);
void read_batch_file(char **input); 
void run_commands(char *input[MAX_ARGS]);
int is_built_in_command(char **input); 
void cd(char **input); 
void clr(char **input); 
void dir(char **input); 
void environ_(char **input);
void echo(char **input); 
void help_func(char **input); 
void pause_(char **input); 
void quit(char **input);
int check_for_redirection(char **input);
void redirection(char **input);
void run_external_command(char **input); 
void exec_ext(char **input);
void check_backend(char *input[MAX_ARGS]);
void check_pipes(char *args[MAX_ARGS]);
void piping(char **input);
char *print_prompter();
char *get_curr_dir();
void shell_loop();

//for input/output redirection
int input_redir;
int output_redir;
int append_redir;
int amp_redir;

//file names for i/o redirection
char *input_file;
char *output_file;

//for backend execution
int backend;
int status;

//for pipes
int piped;
char **input2;

char *commands[] = {"cd", "clr", "dir", "enviro", "echo", "help", "pause", "quit", "path"}; 
char *input_file, *output_file, *p_input, *p_output; 
int input_argc; 

int main(int argc, char *argv[]){ //MAIN METHOD
  if(argc > 1){
    read_batch_file(argv++);
    exit(0);
  }
  //start main loop of shell
  shell_loop();
  return 0;
} //END MAIN METHOD

void run_commands(char *input[MAX_ARGS]) {
  //int built_in =  is_built_in_command(input); 
  //printf("Built in command: %d\n", built_in); 

  //call one built in function
  if(strcmp(input[0], "cd") == 0)  //change directory command
    cd(input);  

  else if(strcmp(input[0], "clr") == 0) //clear command
    clr(input); 

  else if(strcmp(input[0], "dir") == 0) //directory command
    dir(input); 

  else if(strcmp(input[0], "enviro") == 0) //environment command
    environ_(input); 

  else if(strcmp(input[0], "echo") == 0) //echo command
    echo(input); 

  else if(strcmp(input[0], "help") == 0) //help command
    help_func(input); 

  else if(strcmp(input[0], "pause") == 0) //pause command
    pause_(input); 

  else if(strcmp(input[0], "quit") == 0) //quit command
    quit(input);
}

void get_user_input(char *input, char *args[MAX_ARGS]) {
  //printf("%s", "myshell-> "); 
  args[0] = strtok(input, " \n\t");
  for (int i = 1; i < MAX_ARGS-1; i++){
    args[i] = strtok(NULL, " \n\t");
    if (args[i] == NULL)
      return;
  }
}

//check if command is a script file ".txt"
int check_textf(char *input){
  char *temp = input;
  while (*(temp+4) != '\0'){  //check the third to last letter of input
    temp++;
  }
  if(!strcmp(temp, ".txt")) //if last three chars = ."txt"
    return TRUE;
  else 
    return FALSE;
}

//run each line of a .txt file
void run_textf(char *input){
  FILE *file = fopen(input, "r"); //read file
  if (file != NULL){
    char buffer[BUFF]; ////used to hold each line of the file
    while (fgets(buffer, sizeof(buffer), file) != NULL){ //read till end
      char *dir = get_curr_dir();

      printf("\n<BATCH FILE>\n%s%s\n", dir, buffer);

      char *inputs[MAX_ARGS]; //break up line into individual inputs
      get_user_input(buffer, inputs);
      read_batch_file(inputs);
      free(dir);
    }
    printf("\n"); 
    fclose(file);
    return;
  }
  else{
    printf("myshell-> Error, %s could not be opened", input);
  }
}

void read_batch_file(char **input){
  check_for_redirection(input); //check for IO redirection
  check_backend(input); //check for backend execution
  check_pipes(input); //check for pipe commands

  //command is a batch file
  if (check_textf(input[0])){
    run_textf(input[0]);
    return;
  }
  if (piped == TRUE){ //pipe command was detected
    pid_t pipe_pid = fork();
    if (pipe_pid < 0){ //failed
      fprintf(stderr, "%s \n", "myshell-> Error, fork failed");
      exit(0);
    }
    //child
    else if (pipe_pid == 0){
      piping(input);
    } 
    //parent
    else{
      waitpid(pipe_pid, &status, 0); //wait for child to finish
    }
  }
  else if (input_redir == TRUE || output_redir == TRUE || append_redir == TRUE){ //if I/O redirection was found
    pid_t redir_pid = fork();
    if (redir_pid < 0){
      fprintf(stderr, "%s \n", "myshell-> Error, fork failed");
      exit(0);
    }
    //child
    else if (redir_pid == 0){
      redirection(input);
      exit(0);////kill child process
    } 
    //parent
    else{
      waitpid(redir_pid, &status, 0); //wait for child to finish
    }
  }
  else{ //else no pipe or i/o redirection
    run_commands(input); //run normally
  }
}

int quick_error_check(char **input){
  char *cant_have[5] = {"|", ">", "<", ">>", "&"}; //can't have for input[0] or input [n-1]

  for(int i = 0; i < 5; ++i){
    if(strcmp(input[0], cant_have[i]) == 0) {
      fprintf(stderr, "%s %s\n", "myshell: Unexpected token: ", cant_have[i]);
      return 1; 
    }
    if(strcmp(input[input_argc-1], cant_have[i]) == 0) {
      fprintf(stderr, "%s %s\n", "myshell: Unexpected token: ", cant_have[i]);
      return 1; 
    }
  }
  return 0; 
}

//trim spaces not needed *removed*

int is_built_in_command(char **input) { // Looks for a match to run the appropriate command 
  for(int i = 0; i < (sizeof(commands) / sizeof(commands[0])); ++i) {
    //loop through built_in_commands, if found, return true (1) 
    if(strcmp(input[0], commands[i]) == 0)
      return 1; 
  }

  return 0; 
}
 
void clr(char **input) { //Built in command to clear screen
  if(input_argc > 1) {
    fprintf(stderr, "%s \n", "myshell-> Error, clr takes no arguments");
    exit(0); 
  }

  printf("\033[H\033[J"); //https://stackoverflow.com/questions/2347770/how-do-you-clear-the-console-screen-in-c

  exit(0);    
}

void cd(char **input) { // change the directory, needs a second imput
  if(input_argc > 2) {
    fprintf(stderr, "%s \n", "myshell-> Error, too few arguments."); 
    char buffer[100]; 
    printf("%s\n", getcwd(buffer, sizeof(buffer))); //https://www.ibm.com/docs/en/zos/2.3.0?topic=functions-getcwd-get-path-name-working-directory
    exit(0);
  }

  int retval = 0;  

  retval = chdir(input[1]); //chages the directory //when the program is executed in the shell, the shell follows fork on exec mechanism. So, it doesn’t affect the current shell
  //https://www.geeksforgeeks.org/chdir-in-c-language-with-examples/

  if(retval == -1){
    fprintf(stderr, "%s \n", "myshell-> Error, directory not found"); 
    exit(0); 
  }

  exit(0); 
}

void dir(char **input){ //dir <directory> 
  char *name = malloc(sizeof(char) * 150); 
  name = NULL;

  int output_redirection = check_for_redirection(input);

  printf("%d\n", output_redirection);

  FILE *pointer = NULL; 

  if(output_redirection == 1) //1 is <
    pointer = fopen(output_file, "w"); // write to specified output file 
  
  if(output_redirection == 3) //3 is >>
    pointer = fopen(output_file, "a"); // append to specified outputfile 

  if(output_redirection == 1 || output_redirection == 3){
    if(pointer == NULL){
      fprintf(stderr, "%s \n", "myshell-> Error, could not open output file");
      exit(0); 
    }
  }

  if((input_argc > 2) && output_redirection != 1 && output_redirection != 3){ // fix later, multi-lined 
    fprintf(stderr, "%s \n", "myshell-> Error, too many args");
    exit(0); 
  }

  if(input_argc != 2) //print contents of the current directory 
    name = getcwd(name, 150); 

  else if(input_argc == 2) //user 
    name = input[1]; 

    DIR *directory = NULL;
	  struct dirent *directory_entry = NULL; 
    directory = opendir(name); 

    if(directory == NULL)
      fprintf(stderr, "%s \n", "myshell-> Error, <directory> not found");

    // read from directory, print to screen or redirection output 
    while((directory_entry = readdir(directory)) != NULL){
        if(output_redirection == 1 || output_redirection == 3) // specified file is already opened to "w" or "a", just need to write to that file 
          fprintf(pointer, "%s  ", directory_entry->d_name); 
        else
          printf("%s ", directory_entry->d_name);
    }
    if(output_redirection == 1 || output_redirection == 3){
        fprintf(pointer, "%s", "\n"); 
        fclose(pointer);
    }
  exit(0); // success
}


void environ_(char **input){ // list environ strings 
  //https://www.oreilly.com/library/view/c-in-a/0596006977/re105.html
  //get PATH variable
  const char *s = getenv("PATH");
  //if path is NULL
  if (s == NULL) {
    fprintf(stderr, "%s \n", "myshell-> Error, PATH not found");
  }
  //else print out path
  else {
    printf("PATH = %s\n", s);
  }
  exit(0);
}

void echo(char **input) { // echo <string> 
  //loop until end of input
  while (*input != NULL) {
    printf(" %s", *input);
    input++;
  }
  //new line
  printf("\n");
  exit(0);
}

void help_func(char **input){
  printf("%s \n", "myshell-> List of commands and purposes bellow"); 
  printf("%s \n", "cd -> Changes current directory"); 
  printf("%s \n", "dir -> Shows a list of the files in the current directory"); 
  printf("%s \n", "clr -> Clears the terminal"); 
  printf("%s \n", "echo -> Prints out the rest of the arguments");
  printf("%s \n", "enviro -> Prints out the current environment variables");
  printf("%s \n", "quit -> Exits the shell");
  printf("%s \n", "pause -> Pauses shell until user continues (Enter Key)");
  printf("%s \n", "file1 | file2 -> Pipes the output from file1 into file2");
  printf("%s \n", "k < input -> Redirects k's input to input");
  printf("%s \n", "k > output -> Redirects k's output to output"); 
  printf("%s \n", "k >> input -> Appends k's output too output");      
  exit(0);
}

void pause_(char **input){ //pause program until user continues (\n)
  int retval; 
  printf("%s \n", "Press *Enter* to continue..."); 
  while((retval = getchar()) != '\n'){
	  continue; 
  }
  exit(0); 
}

void quit(char **input) { //exit shell 
  printf("%s \n", "Leaving myshell..."); 
  free(input);
  exit(0); 
}

int check_for_redirection (char **args){
  input_redir = FALSE;
  output_redir = FALSE;
  append_redir = FALSE;

  int i = 0;
  //loop through args untill empty or redirection found
  while (args[i] != NULL && i < MAX_ARGS){
    //if output redirection ">"
    if (!strcmp(args[i], ">")){
      output_redir = TRUE;
      args[i] = NULL; //remove redirection arg
      output_file = args[++i]; //save output file arg
      return 1;
    }

    //if output append ">>"
    else if(!strcmp(args[i], ">>")){
      append_redir = TRUE;
      args[i] = NULL;
      output_file = args[++i];
      return 3;
    }

    //if input redirection "<"
    else if(!strcmp(args[i], "<")){
      input_redir = TRUE;
      args[i] = NULL;
      input_file = args[++i];
      return 0;
    }

    //error or not found
    else{
        //printf("%s \n","myshell-> Returning no redirection");
        return 2;
    }
  i++;
  }
}

void redirection(char **input){  //https://pubs.opengroup.org/onlinepubs/009604599/functions/open.html
  //"<", ">", ">>"
  if (input_redir == TRUE){
    int in = open(input_file, O_RDONLY); //Open for reading only
    //if not found
    if (in < 0){
      printf("%s \n","myshell-> Error, input file not found");
      exit(1);
    }

    //replace stdin with input file
    dup2(in, STDIN_FILENO); //https://www.geeksforgeeks.org/dup-dup2-linux-system-call/ 
    close(in);
  }

  //Output redirection
  if (output_redir == TRUE){
    int out = open(output_file, O_WRONLY | O_CREAT, 0666); // opens the file by creating it if neccessary
    //if not found
    if (out < 0){
      puts("myshell-> Error, output file not found");
      exit(0);
    }   
    //replace stdout with output file
    dup2(out, STDOUT_FILENO);
    close(out);
  }

  //Appending output redirection
  else if (append_redir == TRUE){
    int out = open(output_file, O_WRONLY | O_APPEND | O_CREAT, 0666); ////open output file in append mode
    if (out < 0){
      puts("myshell-> Error, output file not found");
      exit(0);
    }
    dup2(out,STDOUT_FILENO); ////replace stdout with output file
    close(out);
  }
  exec_ext(input);
}
void exec_ext(char **input){
  int status;
  pid_t pid = fork();
  //if failed
  if (pid < 0){
    puts("myshell-> Error, fork failed");
    exit(1);
  }

  //child
  else if (pid == 0){
    if (execvp(input[0], input) < 0){
      puts("myshell-> Error, Command not found");
    }

    //child is visabel
    exit(0);
  }

  //parent
  else{
    waitpid(pid, &status, 0);//wait for child to finish
    return;
  }
}

void check_backend(char *input[MAX_ARGS]){
  backend = FALSE;
  //loop through input until "&" is found, or end of input
  int i = 0;
  while (input[i] != NULL){
    //if background char is found
    if (strcmp(input[i], "&") == 0){
      amp_redir = TRUE ;
      input[i] = NULL;
      break;
    }
    i++;
  }
}

void run_external_command(char **input){
  int pid = fork(); //https://www.geeksforgeeks.org/fork-system-call/
  if(pid == -1) {
    fprintf(stderr, "%s \n", "myshell-> Error"); 
  }
  else if(pid == 0)
    execvp(input[0], input); //https://www.geeksforgeeks.org/exec-family-of-functions-in-c/

  else{
    wait(NULL);  
  }
}

void check_pipes(char *args[MAX_ARGS]){
  piped = FALSE;
  input2 = args;
  //loop until pipe command "|" or end of args found
  while (*input2 != NULL){
    if (strcmp(*input2, "|") == 0){
      piped = TRUE;
      *input2 = NULL; // sets "|" to null, to seperate args
      input2++;
      return;
    }
    input2++;
  }  
}

void piping(char **input){ //https://man7.org/linux/man-pages/man2/dup.2.html
  //pipe file descriptors
  int pipefd[2];

  if (pipe(pipefd) == 0) {
    pid_t pid = fork();
    //if fork failed
    if (pid < 0){
      fprintf(stderr, "%s \n", "myshell-> Error"); 
      exit(1);
    }
    //child
    else if (pid == 0) {
      close(1);
      dup2( pipefd[1], 1 ); //use pipe's write end as stdout
      close( pipefd[0] );
      run_commands(input); //execute
      exit(0); //kill child
    }
    //parent
    else {
      close(0);
      dup2( pipefd[0], 0 ); //use pipe's read end as stdin
      close( pipefd[1] );
      run_commands(input2); //execute
      exit(0); //kill parent
    }
  }
}


//*** HELPER Functions for shell_loop() and batch file reading ***//

//Gets the cmd line printed prompt
char *print_prompter(){ //https://www.tutorialspoint.com/c_standard_library/c_function_sprintf.htm
  char cwd[BUFF]; //holds string containing directory
  getcwd(cwd, sizeof(cwd)); //copy current directory into string
  char *temp = malloc(sizeof(char)*BUFF);
  sprintf(temp, "%s-->" , cwd);
  return temp;
}

//Gets Current directory
char *get_curr_dir(){
  char *cwd = malloc(sizeof(char)*BUFF); //holds string containing directory
  getcwd(cwd, sizeof(cwd)); //copy current directory into string
  return cwd;
}

//Everything that gets printed to stdout
void shell_loop(){
  while (TRUE){
    char *input;
    char *args[MAX_ARGS];
    char *prompt = print_prompter();
    input = readline(prompt);
    add_history(input); //void add_history (const char *string) //Place string at the end of the history list. The associated data field (if any) is set to NULL
    get_user_input(input, args);
    if (check_textf(args[0])){ //check for shell script file ".txt"
      run_textf(args[0]);
      free(input);
      continue;
    }

    check_for_redirection(args); //check for IO redirection
    check_backend(args); //check for backend execution
    check_pipes(args); //check for pipe commands
    
    //if pipe command was detected
    if (piped == TRUE){
      pid_t pipe_pid = fork();
      if (pipe_pid < 0){
        fprintf(stderr, "%s \n", "myshell-> Error, fork failed");
        exit(0);
      }
      //child
      else if (pipe_pid == 0){
        piping(args);
      } 
      //parent
      else{
        waitpid(pipe_pid, &status, 0); //wait for child to finish
      }
    }
    else if (input_redir == TRUE || output_redir == TRUE || append_redir == TRUE || amp_redir == TRUE){ //if I/O redirection was found
      pid_t redir_pid = fork();
      if (redir_pid < 0){
        fprintf(stderr, "%s \n", "myshell-> Error, fork failed");
        exit(0);
      }
      //child
      else if (redir_pid == 0){
        redirection(args);
        exit(0); //kill child
      } 
      //parent
      else{
        waitpid(redir_pid, &status, 0); //wait for child to finish
      }
    }
    else{ //run normally
      run_commands(args);
      //fprintf(stderr, "%s \n", "Should go again");
    }
    free(input);
    free(prompt);
  }
}