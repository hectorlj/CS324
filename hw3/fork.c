#include<stdio.h>
#include<stdlib.h>
#include<sys/wait.h>
#include<unistd.h>

int main(int argc, char *argv[]) {
    FILE* fp;
	int pid;
    int child_status;
    int fd[2];
	printf("Starting program; process has pid %d\n", getpid());
    fp = fopen("fork-output.txt", "w");
    fprintf(fp, "%s", "BEFORE FORK\n");
    fflush(fp);
    pipe(fd);
	if ((pid = fork()) < 0) {
		fprintf(stderr, "Could not fork()");
		exit(1);
	}

	/* BEGIN SECTION A */
    fprintf(fp, "%s", "SECTION A\n");
    fflush(fp);
	printf("Section A;  pid %d\n", getpid());
	//sleep(30);

	/* END SECTION A */
	if (pid == 0) {
		/* BEGIN SECTION B */
        fprintf(fp, "%s", "SECTION B\n");
        fflush(fp);
        close(fd[0]);
        FILE * fdb = fdopen(fd[1],"a");
        fputs("hello from Section B\n", fdb);
        fflush(fp);
		printf("Section B\n");
		//sleep(30);
        //sleep(30);
		printf("Section B done sleeping\n");
        printf("Program \"%s\" has pid %d. Sleeping.\n", argv[0], getpid());
        //sleep(30);
        
        char *newenviron[] = { NULL };

	    printf("Program \"%s\" has pid %d. Sleeping.\n", argv[0], getpid());
	    //sleep(30);

	    if (argc <= 1) {
		    printf("No program to exec.  Exiting...\n");
		    exit(0);
	    }

	    printf("Running exec of \"%s\"\n", argv[1]);
	    dup2(fileno(fdb), 1); 
        execve(argv[1], &argv[1], newenviron);
	    printf("End of program \"%s\".\n", argv[0]);


		exit(0);

		/* END SECTION B */
	} else {
        char str[60];
		/* BEGIN SECTION C */
        wait(&child_status);
        close(fd[1]);
        FILE * fdc = fdopen(fd[0], "r");
        fgets(str, 60, fdc);
        puts(str);
        fclose(fdc);
        fprintf(fp, "%s", "SECTION C\n");
        fflush(fp);
		printf("Section C\n");
		//sleep(30);
        //sleep(30);
		printf("Section C done sleeping\n");

		exit(0);

		/* END SECTION C */
	}
	/* BEGIN SECTION D */
    fprintf(fp, "%s", "SECITON D\n");
    fflush(fp);
	printf("Section D\n");
	//sleep(30);

	/* END SECTION D */
}
