#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#define bufSize 9
int main(int argc, char *argv[]){
    FILE * fp;
    char buf[bufSize];
    int st = 1;
    char * env;
    env = getenv("SLOWCAT_SLEEP_TIME");
    if(env != NULL){
        st = atoi(env);
    }
    if(strcmp(argv[1], "-") == 0){
        fp = stdin;    
    }
    else {
        fp = fopen(argv[1], "r");
    }
    
    
    fprintf(stderr, "The process id: %d\n", getpid());
    while(fgets(buf, 9, fp)){
        fprintf(stdout,"%s\n", buf);
        sleep(st);
    }
    if(strcmp(argv[1], "-") < 0){
        fclose(fp);
    }
    return 0;
}
