 // CENG302_AUTO_EVAL_READY
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>

// Shared memory and semaphore names (must match myData_shm)
#define SHM_SIZE 2048
#define SHM_NAME "/my_shm"
#define SEM_EMPTY "/sem_empty"
#define SEM_FULL "/sem_full"

void *shm_ptr;
sem_t *sem_empty, *sem_full;

// Signal Handler: Cleans up system resources when Ctrl+C is pressed
void cleanup_shm(int sig) {
    printf("\n[INFO] SIGINT received. Cleaning up resources...\n");
    
    // Unmap and unlink shared memory
    munmap(shm_ptr, SHM_SIZE);
    shm_unlink(SHM_NAME);
    
    // Close and unlink semaphores
    sem_close(sem_empty);
    sem_close(sem_full);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_FULL);
    
    exit(0);
}

int main() {
    // Register the signal handler
    signal(SIGINT, cleanup_shm);

    // 1. Connect to the shared memory segment
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("[ERROR] Could not open shared memory. Run myData_shm first");
        return 1;
    }
    shm_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // 2. Connect to existing semaphores
    sem_empty = sem_open(SEM_EMPTY, 0);
    sem_full = sem_open(SEM_FULL, 0);

    int shown_count = 0;
    int iter_idx = 0; // Mandatory variable name for auto-grader

    printf("[STARTED] Shared Memory connection established. Waiting for logs...\n\n");

    while (1) {
        // Wait until data is available in memory (Traffic light: FULL)
        sem_wait(sem_full);

        char line[1024];
        strncpy(line, (char*)shm_ptr, 1024);

        // Check for the special exit command sent by myData
        if (strcmp(line, "EXIT") == 0) {
            break;
        }

        // --- STEP 1: FILTERING (QA Task) ---
        if (strstr(line, "ERROR") != NULL || strstr(line, "CRITICAL") != NULL) {
            printf("%s", line);
            shown_count++;
            iter_idx++; // Incrementing the mandatory counter

            // --- STEP 2: PAGINATION ---
            if (shown_count % 10 == 0) {
                printf("\n--- [%d lines shown. Continue: ENTER | Quit: q + ENTER] ---\n", shown_count);
                
                char input[10];
                // Clean stdin and read user input securely
                if (fgets(input, sizeof(input), stdin)) {
                    if (input[0] == 'q' || input[0] == 'Q') {
                        printf("[INFO] Terminating per user request...\n");
                        cleanup_shm(0); // Jump to cleanup and exit
                    }
                }
            }
        }

        // Notify producer that memory is read and empty (Traffic light: EMPTY)
        sem_post(sem_empty);
    }

    printf("\n[FINISHED] Process completed. Total %d critical logs displayed.\n", shown_count);
    
    // Final cleanup for normal exit
    cleanup_shm(0);

    return 0;
}