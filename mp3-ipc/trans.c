#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

/* BLOCK_SIZE is set to 4kb */
#define BLOCK_SIZE 4096

/* READ_END / WRITE_END are used to identify pipe ends */
#define READ_END 0
#define WRITE_END 1

/* block_info is a struct that contains the block number and block length */
typedef struct {
    int block_num;
    ssize_t block_len;
} block_info;


int main(int argc, char *argv[]) {
	const char *name = "ee20z_cop4610"; // Shared memory name

    /* infd and outfd are the input and output file descriptors */
    // infd is used to write to shared memory
    // outfd is used to write to output file from shared memory
    int infd, outfd;
    if(argc != 3){
        printf("Usage: ./trans <input file> <output file>");
    }

    // check if output file is empty
    if(access(argv[2], F_OK) != -1) {
        printf("Output file is not empty. Overwrite? (y/n): ");
        char c;
        scanf("%c", &c);
        while(c != 'y' && c != 'n') {
            printf("Invalid input. Overwrite? (y/n): ");
            scanf("%c", &c);
        }
        // If user does not want to overwrite, exit program
        if(c != 'y') {
            printf("Exiting program...\n");
            exit(0);
        }
        // Else, print message and continue
        else{
            printf("Overwriting output file...\n");
        }
    }

    /* Open input and output files */
    // infd is set to read only
    infd = open(argv[1], O_RDONLY);
    assert(infd >= 0 && "Input file open error");
    // outfd is set to read write, create if not exist, truncate if exist
    outfd = open(argv[2], O_RDWR|O_CREAT|O_TRUNC, 0666);
    assert(outfd >= 0 && "Output file open error");

    /* Create two pipes */
    int parent_to_child[2]; // pipe for parent to child
    int child_to_parent[2]; // pipe for child to parent
    if (pipe(parent_to_child) == -1) {
        printf("pipe(parent_to_child) failed%s\n", strerror(errno));
        exit(1);
    }
    if (pipe(child_to_parent) == -1) {
        printf("pipe(child_to_parent) failed%s\n", strerror(errno));
        exit(1);
    }

    /* Create a child process */
    pid_t pid = fork();
    if (pid < 0) {
        printf("fork failed%s\n", strerror(errno));
        exit(1);
    }

    /**************************************************************************************
     * Child process: Opens shared memory, reads 4kb of data stored in memory, then writes 
     * data to output file. Process reads signal sent by parent process to know when to read. 
     * Child sends a signal to parent process to know when it is done writing to output file.
     **************************************************************************************/
    if (pid == 0) { // Child process
        // close unused ends of the pipes
        close(parent_to_child[WRITE_END]);
        close(child_to_parent[READ_END]);
        int shm_fd;
        void *ptr;
        ssize_t writecnt;
    

        /* the BLOCK_SIZE (in bytes) of shared memory object */
        /* open the shared memory object */
        shm_fd = shm_open(name, O_RDONLY, 0666);
        assert(shm_fd >= 0 && "Shared memory open error");

        ftruncate(shm_fd, BLOCK_SIZE);
        assert(shm_fd >= 0 && "Shared memory open error");
        
        /* memory map the shared memory object */
        ptr = mmap(0,BLOCK_SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);
        assert(ptr != MAP_FAILED && "Shared memory map error");

            
        /**************************************************************************************
         * While loop: Reads 4kb of data from input file, writes data to shared memory, then
         * sends signal to parent process to read data from shared memory. Child process reads
         * signal from parent process to know when to write to output file or when to exit.
         **************************************************************************************/
        block_info info;
        while(read(parent_to_child[READ_END], &info, sizeof(info)) >= 0){
            if(info.block_len == 0 && info.block_num == 0){
                write(child_to_parent[WRITE_END], &info.block_num, sizeof(info.block_num));
                break;
            }
            writecnt = write(outfd, ptr, info.block_len);
            assert(writecnt == info.block_len && "Write error");

            int signal_block = info.block_num;
            write(child_to_parent[WRITE_END], &signal_block, sizeof(int));
            fsync(child_to_parent[WRITE_END]);
        }
        
        // close pipe ends
        close(parent_to_child[READ_END]);
        close(child_to_parent[WRITE_END]);
        // close shared memory
        munmap(ptr, BLOCK_SIZE);

        /* remove the shared memory object */
        if(shm_unlink(name) == -1) {
            printf("Error removing %s: %s\n",name,strerror(errno));
            exit(-1);
        }


    /**************************************************************************************
     * Parent process: Opens shared memory, reads 4kb of data from input file, then writes
     * data to shared memory. Parent sends a signal to child process to know when it is done
     * reading from shared memory.
     **************************************************************************************/
    } else { // Parent process
        // close unused ends of the pipes
        close(parent_to_child[READ_END]);
        close(child_to_parent[WRITE_END]);
        int shm_fd;
        void *ptr;

        // /* create the shared memory segment */
        shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
        assert(shm_fd >= 0 && "Shared memory open error");

        /* configure the BLOCK_SIZE of the shared memory segment */
        ftruncate(shm_fd, BLOCK_SIZE);

        /* now map the shared memory segment in the address space of the process */
        ptr = mmap(0,BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        assert(ptr != MAP_FAILED && "Shared memory map error");

        /**************************************************************************************
         * While loop: Reads 4kb of data from input file, writes data to shared memory, then
         * sends signal to child process to read data from shared memory. Parent process reads
         * signal from child process to know it has compelte writing to output file. Parent
         * process exits when child process sends exit signal.
         **************************************************************************************/
        int block_num = 1; // Block number
        ssize_t block_bytes; // Block size
        while((block_bytes = read(infd, ptr, BLOCK_SIZE)) >= 0){
            if(block_bytes > 0){
                block_info info = {block_num, block_bytes};

                // send block_num and block_bytes to the child process
                write(parent_to_child[WRITE_END], &info, sizeof(info));

                // wait for the child process to finish and send signal back
                int signal_block = 0;
                read(child_to_parent[READ_END], &signal_block, sizeof(int));
                fsync(child_to_parent[READ_END]);
                // check if child process sent correct signal
                assert(signal_block == info.block_num && "Signal error");
                
                // increment block number
                block_num++;
            }
            // if there is no more data to read, send exit signal to the child process
            else {
                block_info info = {0, 0};
                write(parent_to_child[WRITE_END], &info, sizeof(info));
                break;
            }

        }

        // close pipe ends
        close(parent_to_child[WRITE_END]);
        close(child_to_parent[READ_END]);
        
        waitpid(pid, NULL, 0);

        munmap(ptr, BLOCK_SIZE);
    }
    return 0;
}
