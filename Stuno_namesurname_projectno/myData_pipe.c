// CENG302_AUTO_EVAL_READY
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> //to be able to make more than one thread
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <signal.h> 


pthread_mutex_t pipe_lock; //to make sure our threads dont intervene with each other, only the ones that have mutex can write through the pipe
int pipe_fd; 

//cleaning function
void signal_handler(int sig) {
    printf("\n[Closing] Signal %d was received. Cleaning...\n", sig);
    unlink("/tmp/my_log_pipe"); //deletes the pipe file
    exit(0);
}

void* log_reader_thread(void* arg) {
    char* filename = (char*)arg; // we are sending the file name from the main function, but since we took it as a void
    //in the code before we are turning this to a char for fopen
    FILE* file = fopen(filename, "r"); //thread wsnts to read the file "r"
    //this is called type casting
    if (!file) {
        perror("Dosya acilamadi");
        return NULL;
    }

    char line[1024]; //before we read the line from the file we have to keep it in a temporary place 
    
    while (fgets(line, sizeof(line), file)) { //reading
        
        

        pthread_mutex_lock(&pipe_lock); //we are useing the lock mutex that we wrote before in here
        //we are doing this because the pipe only has one openining and cant endure more than 1 thread.
        // so if there is an thread working in the entrance of the line its say,ng that its in use and locks it
        //RESERVATION!!

        
        //writing to the pipe to send it to mymore
        write(pipe_fd, line, strlen(line)); 
        //pipe_fd=tunnel entrance 
        pthread_mutex_unlock(&pipe_lock);
        // its done so unlock the pipe
       
    }

    fclose(file);
    printf("[INFO] %s file has been read and sent to the pipe.\n", filename);
    return NULL;
}
//argc= argument count, counts the word count of the argument
//argv= argument vector, list that keeps every word that has been written to the command line
//argv[0]= name of the program, [1],[2] is the file names that we have
int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler); //activate the "ctrl+c" catcher
    // the project wants us to take multiple log files as command-line arguments

    // argument count --> parameter count. if we wrote one word: ./myData_pipe the count will become 1,
    // but since we need a file to read it from not just the name of the program we have to write at least 2 words such as: ./myData_pipe dosya1.txt
    // so we need at least argc 2!
    if (argc < 2) {
        printf("Usage: %s log1.txt log2.txt ...\n", argv[0]);
        return 1; //end thep rogram till they write the names of the files
    }

    // naming our pipe
    char *myfifo = "/tmp/my_log_pipe"; //tempoaray file folder so that can be reachable from the both sides of the pipe
    //my_log_pipe is the name of our pipe
    mkfifo(myfifo, 0666); // make fifo, our adress to myfifo, 0666 everyone can access to our pipe witgh this code
    
    // before openng the pipe we are making sure that its locked and allows only one thread at a time
    pthread_mutex_init(&pipe_lock, NULL);
    
    //to see if the pipe has opened we are printing
    printf("[START] Pipe is opening, waiting for myMore_pipe...\n");
    
    //only writes to the pipe
    pipe_fd = open(myfifo, O_WRONLY);

   
    int num_files = argc - 1; //1 of them is program name so we dont count it the others are files
    // creats threads as much as needed( smae as the file count)
    pthread_t threads[num_files];

    for (int i = 0; i < num_files; i++) {
        // creating a thread for every file 
        // creating a thread and giving it a job
        //&threads[i]: this is the ith worker and thats its id, its going to read the file nsmed argv[i+1](argv[0] is the programs name [1]is the first files')
        if (pthread_create(&threads[i], NULL, log_reader_thread, (void*)argv[i+1]) != 0) {
            perror("Thread couldnt be created");
        }
    }

    
    //pthread_join: stops main there and says wait and join, makes sure that main program doesnt close untill threads finish their jobs
    for (int i = 0; i < num_files; i++) {
        pthread_join(threads[i], NULL);
    }

  
    close(pipe_fd); 
    pthread_mutex_destroy(&pipe_lock); //unlock
    printf("[DONE] everything is done.\n");
    
    return 0;
}