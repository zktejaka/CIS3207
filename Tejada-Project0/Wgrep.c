//searches a file for a sequence of characters, printing each line that contains that sequence
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]){
    
    if(argc == 1){ // If it is 1, exit with a status of 1
        printf("wgrep: searchterm [file ...]\n");
        exit(1);
    }
    else if(argc == 2){ //If it is 2, treat the first argument as the search term, and searchusing stdin instead of an open FILE*
        char *line, *res = NULL;
        size_t len = 0; 
        int read = -1; 
  
        printf("%s", "Type a sentence to see if your searchterm is in it: "); 

        while((read = getline(&line, &len, stdin)) != -1){
            res = strstr(line, argv[1]);
            if(res != NULL){ // if term found, print the whole line
                printf("%s", line);
                free(line);
                line = NULL;
                len = 0; 
            }
        }
    // will keep reading from stdin if manually enter input (./wgrep searchterm), ctrl-c to stop 
    }
    for(int i=2; i<argc; ++i){ //f it is > 2, treat the first argument as the search tearm, and treat the rest of the arguments as search files
        FILE *file = NULL;
        file = fopen(argv[i], "r"); //read file
        
        if(file == NULL){ // can't open file, end program
            printf("wgrep: searchterm [file ...]\n");
            exit(1);
        }
        // use getline() to read whole line from specified file 
        char *line, *res = NULL; 
        size_t len = 0; 
        int read = -1; // will hold length of line (not including '\0'), or getline() returns -1 if EOF or failure to read 

        while((read = getline(&line, &len, file)) != -1){
            res = strstr(line, argv[1]);
            if(res != NULL){ // term found, print whole line
                printf("%s", line);
                free(line);
                line = NULL;
                len = 0; 
            }
        }

        int status = fclose(file); // close file
        if(status == EOF){
            printf("error: failed to close file");
        }
    }
    return 0; 
}