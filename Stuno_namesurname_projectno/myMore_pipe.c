// CENG302_AUTO_EVAL_READY
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>

#define PAGE_SIZE 10  // how many filtered lines to show before pausing

int pipe_fd;  // file descriptor for the named pipe

// signal handler for Ctrl+C (SIGINT)
// closes the pipe and deletes the FIFO file before exiting
void handle_sigint(int sig) {
    printf("\n[INFO] SIGINT received. Cleaning up and exiting...\n");
    close(pipe_fd);
    unlink("/tmp/my_log_pipe");
    exit(0);
}

// checks if a log line contains ERROR or CRITICAL
// returns 1 if it should be shown, 0 if it should be skipped
int is_important(char *line) {
    if (strstr(line, "ERROR") != NULL) return 1;
    if (strstr(line, "CRITICAL") != NULL) return 1;
    return 0;
}

// waits for user input after every PAGE_SIZE lines
// returns 1 to continue, 0 to quit
int ask_user() {
    printf("--- [Press Space+Enter to continue | q+Enter to quit] ---\n");
    char input[64];
    if (!fgets(input, sizeof(input), stdin)) return 0;  // EOF
    if (input[0] == 'q' || input[0] == 'Q') return 0;  // user wants to quit
    return 1;  // anything else means continue
}

int main() {
    char *myfifo = "/tmp/my_log_pipe";  // same pipe name as myData_pipe

    // register SIGINT handler so Ctrl+C doesn't crash the program
    signal(SIGINT, handle_sigint);

    printf("[START] Waiting for myData_pipe to open the pipe...\n");

    // open the pipe for reading
    // this will block here until myData_pipe opens the write end
    pipe_fd = open(myfifo, O_RDONLY);
    if (pipe_fd < 0) {
        perror("Could not open pipe");
        return 1;
    }

    printf("[INFO] Connected to pipe. Filtering logs (ERROR | CRITICAL only)...\n\n");

    char ch;
    char line[1024];
    int pos = 0;            // current position in line buffer
    int shown = 0;          // how many filtered lines shown on current page
    int total = 0;          // total filtered lines shown

    // read the pipe character by character and build lines
    while (read(pipe_fd, &ch, 1) > 0) {
        if (ch == '\r') continue;  // skip Windows-style carriage returns

        if (ch == '\n' || pos >= 1023) {
            line[pos] = '\0';  // null-terminate the line
            pos = 0;           // reset buffer for next line

            // skip lines that are not ERROR or CRITICAL
            if (!is_important(line)) continue;

            printf("%s\n", line);
            shown++;
            total++;

            // after every PAGE_SIZE lines, wait for user input
            if (shown >= PAGE_SIZE) {
                shown = 0;
                if (!ask_user()) {
                    printf("[INFO] User quit.\n");
                    break;
                }
            }
        } else {
            line[pos++] = ch;  // add character to line buffer
        }
    }

    // if there were leftover characters without a newline at the end
    if (pos > 0) {
        line[pos] = '\0';
        if (is_important(line)) {
            printf("%s\n", line);
            total++;
        }
    }

    if (total == 0)
        printf("[INFO] No ERROR or CRITICAL logs found.\n");
    else
        printf("\n[DONE] %d lines shown in total.\n", total);

    close(pipe_fd);
    unlink(myfifo);  // clean up the FIFO file
    return 0;
}
