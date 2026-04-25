// CENG302_AUTO_EVAL_READY
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>

//constants and names
#define SHM_SIZE 2048          //shared memory size
#define SHM_NAME "/my_shm"     //memory name
#define SEM_EMPTY "/sem_empty" //empty semaphor for the reader
#define SEM_FULL "/sem_full"   //full semaphor for the reader

//global variables (to be able to do the cleaning with the signals)
void *shm_ptr;
sem_t *sem_empty, *sem_full;
pthread_mutex_t shm_lock;

//signal catcher
void cleanup_and_exit(int sig) {
    printf("\n[Kapatılıyor...] Bellek ve semaforlar temizleniyor.\n");
    munmap(shm_ptr, SHM_SIZE);
    shm_unlink(SHM_NAME);
    sem_close(sem_empty);
    sem_close(sem_full);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_FULL);
    exit(0);
}

//writes data to memory
void* log_writer_thread(void* arg) {
    char* filename = (char*)arg;
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Dosya acilamadi");
        return NULL;
    }

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        sem_wait(sem_empty); //wait for an empty slot
        
        pthread_mutex_lock(&shm_lock); //prevent other threads from interfering
        
        memset(shm_ptr, 0, SHM_SIZE);  //clean the previous data
        strncpy((char*)shm_ptr, line, SHM_SIZE); //copy data to memory
        
        pthread_mutex_unlock(&shm_lock);
        
        sem_post(sem_full);  //send ready signal to the reader
    }

    fclose(file);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Kullanım: %s log1.txt log2.txt ...\n", argv[0]);
        return 1;
    }

    //create the signal catcher
    signal(SIGINT, cleanup_and_exit);

    //create the shared memory
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, SHM_SIZE);
    shm_ptr = mmap(0, SHM_SIZE, PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // SEM_EMPTY: 1 (ready to write), SEM_FULL: 0 (nothing to read)
    sem_empty = sem_open(SEM_EMPTY, O_CREAT, 0666, 1);
    sem_full = sem_open(SEM_FULL, O_CREAT, 0666, 0);

    pthread_mutex_init(&shm_lock, NULL);

    //create the threads
    int num_files = argc - 1;
    pthread_t threads[num_files];

    printf("[START] Shared Memory hazır. Okuyucu (myMore_shm) bekleniyor...\n");

    for (int i = 0; i < num_files; i++) {
        pthread_create(&threads[i], NULL, log_writer_thread, (void*)argv[i+1]);
    }

    for (int i = 0; i < num_files; i++) {
        pthread_join(threads[i], NULL);
    }

    //send closing signal to the reader
    sem_wait(sem_empty); //wait for an empty slot to write
    pthread_mutex_lock(&shm_lock);
    
    memset(shm_ptr, 0, SHM_SIZE);
    strncpy((char*)shm_ptr, "EXIT", SHM_SIZE); //reader will stop when it sees "EXIT"
    
    pthread_mutex_unlock(&shm_lock);
    sem_post(sem_full); //notify the reader for the last package has arrived

    //making sure the reader read the "EXIT" signal
    sleep(1);

    //if program ends with no conventions, do the cleaning
    cleanup_and_exit(0);

    return 0;
}