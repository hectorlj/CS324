#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main (int argc, char *argv[]){
    char *newenviron[] = { NULL };

    if(argc <= 1){
        printf("No program to exec. Exiting...\n");
        exit(0);
    }
    printf("Running exec of \"%s\"\n", argv[1]);
    execve(argv[1], &argv[1], newenviron);
    
}
