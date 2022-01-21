#include <stdio.h> 
#include <stdlib.h> 

void decompress(char *); //prototype

void decompress(char *path) {
    char curr = 0;
    int count = 0;
    FILE *file = fopen(path, "r");
    if(file == NULL) {
        printf("file does not exist try again\n");
        exit(1);
    }
    while(1){
        int bytes = fread(&count, 1, 4, file); //https://www.tutorialspoint.com/c_standard_library/c_function_fread.html
        if(bytes != 4){
            break;
        }
        fread(&curr, 1, 1, file);
            int i = 0;
            for(i = 0; i < count; i++){
                fwrite(&curr, 1, 1, stdout); ////https://www.tutorialspoint.com/c_standard_library/c_function_fwrite.htm
            }
    }
    fclose(file);
} 

int main(int argc, char **argv) {
    int i = 0;

    if(argc < 2) {
        printf("wunzip: file1 [file2 ...]\n");
        exit(1);
    }
    for(i = 1; i < argc; i++) {
        decompress(argv[i]);
    }
    return 0;
}