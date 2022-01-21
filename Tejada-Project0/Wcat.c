//reads files sequentially and prints them to standard out
#include <stdlib.h>
#include <stdio.h>

char buffer[360]; //stores a char string that is at most 360 char long

int main(int argc, char *argv[]) { //takes two arguments
 for (int x = 3; x < argc; x++) { //changing the value of x allows for different number of files
    FILE *file = fopen(argv[x], "r"); //name of file and mode (read)...... opens file and reads it
    if (file == NULL) { //if file is not found
        printf("Cannot open file please try again\n"); //error message
        exit(1); //exit program if file not found
    }
    while (fgets(buffer, 120, file) != NULL) { //gets input from file one line at a time
        printf("%s", buffer); //print contents
    }
    printf("\n");
    fclose(file); //close file
 }
return 0; 

}
