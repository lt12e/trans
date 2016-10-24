//trans.c
//Laura Ann Truax
//lt12e

/*Your code must work with both binary and text files. For example, you can test it with the generated
trans file, which is a binary file. You may use fread/fwrite to access files. It should also print out
a helpful error message. It should also promote the user whether to overwrite the file if the output file
already exists.
*/

#include <stdio.h>		//for handling files
#include <unistd.h>		//for pid_t, fork
#include <stdlib.h> 	//for exit()
#include <sys/types.h> 	//for getpid and getppid
#include <sys/wait.h>	//for wait
#include <sys/shm.h>	//for shm
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h> 	//for strerror, etc


int main(int argc, char *argv[])	{
	int sendpipefd[2];		//pipe file descriptors (upload from parent)
	int returnpipefd[2];	//pipe file descriptors (download to parent)
	pid_t cpid;				//pid_t for child process
	char buf;				//file buffer  ?

	FILE *infile;
	FILE *outfile;
	char *infilename;
	char *outfilename;

	const char *shm_file = "/shm-lt12e";	//shared mem file name
	const int SIZE = 256;					//shared mem file size. Make this sizeof(int)*4?
	int trunc;

	int shm_fd;		//shared memory file descriptor
	char *shm_base;	//base address
//	char *ptr;		//pointer to base address

	char block[4];
	int blocknum = 1;	//TODO change to -1?
	int blocksize = 0;

//Check for the proper amount of args
	if(argc != 3){
		printf("Usage: trans <file1> <file2>");
		exit(EXIT_FAILURE);
	}

	//create two Linux pipes (up pipe and down pipe).
    if (pipe(sendpipefd) == -1) {
        printf("Creation of send pipe failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pipe(returnpipefd) == -1) {
        printf("Creation of return pipe failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

	//a block of data is 4kb or less
	//create shared memory block
	shm_fd = shm_open(shm_file, O_CREAT | O_RDWR, 0666);	//TODO change mode to 0600?
	if(shm_fd == -1){
		printf("Shared memory failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	trunc = ftruncate(shm_fd, SIZE);	//Set size of shared memory
	if (trunc != 0){
		printf("ftruncate failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/*The entire virtual address space of the parent is replicated in the child, including the states of mutexes,
	 * condition variables, and other pthreads objects; the use of pthread_atfork(3) may be helpful for dealing
	 * with problems that this can cause. */
	cpid = fork();	//create child process using fork

	if(cpid < 0){
		printf("Fork failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* create shm after fork */
	shm_base = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if(shm_base == MAP_FAILED){	//TODO change to == -1?
		printf("Map failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

//	ptr = *shm_base;

	//Try to open infile
	infilename = argv[1];
	infile = fopen(argv[1], "rb");	//TODO, add "rb" mode?
	//If unable to infile, print error message
	if(infile == NULL){		//TODO Prints twice?
		printf("Unable to open input file: %s\n", infilename);
		exit(EXIT_FAILURE);
	}

	//Try to open outfile
	outfile = fopen(argv[2], "wb");
	outfilename = argv[2];
	//If unable to open outfile, print error message
	if(outfile == NULL){
		printf("Unable to open output file: %s\n", outfilename);
		exit(EXIT_FAILURE);
	}

	//TODO If output file is not empty, prompt user to clear it
	//TODO change fgetc to something that gets a block (4 chars) of data ?
	//TODO check for shared memory errors

	//The last block's number is 0. It is an empty block that acts as a terminator. The parent will send it to the child and the child will send the block number back and exit.
	//When the parent receives 0 as a reply, it exits.

	if (cpid == 0) {            /* Child reads from pipe and shm*/
		if(close(sendpipefd[1]) == -1){
			printf("Unable to close sendpipefd[1]: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		if(close(returnpipefd[0]) == -1){
			printf("Unable to close returnpipefd[0]: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}

		while(blocknum != 0){
			if((read(sendpipefd[0], &blocknum, sizeof(blocknum))) == -1){
				printf("Child block number read from pipe 1 failed: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}

			if((read(sendpipefd[0], &blocksize, sizeof(blocksize))) == -1){
				printf("Child block size read from pipe 1 failed: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}

printf("Child read num, size from the pipes: %d, %d\n", blocknum, blocksize);
			if((write(returnpipefd[1], &blocknum, sizeof(blocknum))) == -1){
				printf("Child write to pipe 2 failed: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			else{
				printf("Child wrote to return pipe: %d\n", blocknum);
			}

		}//end child while loop


		if(close(sendpipefd[0]) == -1){
			printf("Unable to close sendpipefd[0]: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		if(close(returnpipefd[1]) == -1){
			printf("Unable to close returnpipefd[1]: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}

printf("Child process is exiting...\n");
		exit(EXIT_SUCCESS);

	}	//end child process section

	else {    /* Parent writes to pipe and shm*/

		if(close(sendpipefd[0]) == -1){
			printf("Unable to close sendpipefd[0] in parent: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		if(close(returnpipefd[1]) == -1){
			printf("Unable to close returnpipefd[1] in parent: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}

		while(blocknum != 0){
			blocksize = 0;	//reset block size
			for(int i=0; i<4; ++i){
				block[i] = '\0';
			}
			for(int i=0; i<4; ++i){		//populate the block and get the block size
				if((buf = fgetc (infile) ) != EOF){ //else, eof
					block[i] = buf;
					blocksize++;
				}
			}

			if(block[0] == '\0')	//block is empty, send a 0 for blocknum to indicate end of input
				blocknum = 0;

	//TODO remove printf statements
				printf("\nBlock: ");
				for(int i=0; i<4; ++i)
					printf("%d ",block[i]);

	printf("\nBlocknum currently is: %d\n", blocknum);

				if((write(sendpipefd[1], &blocknum, sizeof(blocknum))) == -1){
					printf("Parent blocknum write to pipe 1 failed: %s\n", strerror(errno));
					exit(EXIT_FAILURE);
				}
				else
					printf("Parent wrote blocknum to pipe: %d\n",blocknum);

				if((write(sendpipefd[1], &blocksize, sizeof(blocksize))) == -1){
					printf("Parent blocksize write to pipe 1 failed: %s\n", strerror(errno));
					exit(EXIT_FAILURE);
				}
				else
					printf("Parent wrote blocksize to pipe: %d\n",blocksize);


	printf("Parent is reading from the return pipe\n");
				if((read(returnpipefd[0], &blocknum, sizeof(blocknum))) == -1){
					printf("Parent read from pipe 2 failed: %s\n", strerror(errno));
					exit(EXIT_FAILURE);
				}



	printf("Parent read blocknum from pipe: %d\n", blocknum);
			if(blocknum > 0)
				blocknum++;
	printf("Blocknum increased to: %d\n", blocknum);

		}//end while loop

		wait(NULL);	//Wait for child process to exit

		if(close(sendpipefd[1]) == -1){
			printf("Unable to close sendpipefd[1]: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		if(close(returnpipefd[0]) == -1){
			printf("Unable to close returnpipefd[0]: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
printf("Parent process is returning to main...\n");
	}	//end parent process section


	//Close the input and output files
	fclose(infile);
	fclose(outfile);

/*Make sure the input and output files are the same after running trans.  You can compare their
checksums using shasum
 */

    //TODO int munmap(void *addr, size_t length);
	//TODO which order do these go in?

	if(shm_unlink(shm_file) == -1){		//TODO no such file or directory (NO shm_link CREATED)
		printf("shm_unlink failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if(munmap(0, SIZE) == -1){
		printf("munmap failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if(close(shm_fd) == -1){
		printf("close(shm_fd) failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

//	exit(EXIT_SUCCESS);
	return 0;
}//end of main
