#include <stdio.h>
#include <stdlib.h>

void compress(FILE *); //prototype

void compress(FILE *stream){ //function that actually does the compressing
  int count = 1;
  char last = '\0'; //null to tell when end of string
  char current = fgetc(stream); //https://www.tutorialspoint.com/c_standard_library/c_function_fgetc.htm

  while (current != EOF){
    if (current == last)
      count++;
    else{
      fwrite(&count, sizeof(count), 1, stdout); //https://www.tutorialspoint.com/c_standard_library/c_function_fwrite.htm
      fwrite(&last, sizeof(last), 1, stdout); //stdout is standard output
      count = 1;
    }
    last = current;
    current = fgetc(stream);
  }
}

int main(int argc, char *argv[]){
  if (argc == 1){
    printf("wzip: file1 [file2]"); //command not issued correctly
    exit(1);
  }

  FILE *file;
  int i = 1;

  //loop to open and use the file(s)
  while (i < argc){
    file = fopen(argv[i], "r");
    //checking if the file was opened successfully
    if (file == NULL){
      printf("File cannot be opened.");
      exit(1);
    }
    compress(file); //call to seperate function
    fclose(file);
    i++; //next file is appliciable
  }
  exit(0);
}