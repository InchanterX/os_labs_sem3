#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include "writer.h"

#define ERR_STD_FD_CLOSED 125
#define ERR_SETUP_FAILED 126
#define ERR_EXEC_FAILED 127
#define ERR_FLAG_MEM (1 << 0)
#define ERR_FLAG_WRITE (1 << 1)
#define ERR_FLAG_CHILD1 (1 << 2)
#define ERR_FLAG_CHILD2 (1 << 3)
#define ERR_FLAG_IO (1 << 4)


int check_std_fds(void) {
    int ok = 1;
    for (int fd = 0; fd <= 2; fd++) {
        if (fcntl(fd, F_GETFD) == -1) {
            fprintf(stderr, "Standard descriptor %d is closed.\n", fd);
            ok = 0;
        }
    }
    return ok ? 0 : 1;
}

char *dynamic_input_reading(FILE *std_in_stream, size_t *out_length) {
    if (!std_in_stream || !out_length) return NULL;
    size_t size = 128;
    size_t length = 0;
    char *buffer = malloc(size);
    if (!buffer) return NULL;

    int piece;
    while ((piece = fgetc(std_in_stream)) != EOF) {
        if (length + 1 >= size) {
            size *= 2;
            char *new = realloc(buffer, size);
            if (!new) {
                free(buffer);
                return NULL;
            }
            buffer = new;
        }
        buffer[length++] = (char)piece;
    }

    buffer[length] = '\0';
    *out_length= length;
    return buffer;
}

int wait_for_child_status(pid_t pid, const char *child_name) {
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("Waitpid failed");
        return 1;
    }

    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);

        if (exit_code == ERR_EXEC_FAILED) {
            fprintf(stderr, "Child (%s) was unable to execute exec.\n", child_name);
            return 1;
        } else if (exit_code == ERR_SETUP_FAILED) {
            fprintf(stderr, "Child (%s) failed to set up file descriptors and never started.\n", child_name);
            return 1;
        } else if (exit_code != 0) {
            fprintf(stderr, "Child (%s) started but finished with an error.\n", child_name);
            return 1;
        }
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "Child (%s) process was killed with a signal %d.\n", child_name, WTERMSIG(status));
        return 1;
    }
    return 0;
}

int print_from_pipe(int pipe_fd, size_t block_size) {
    char *buffer = malloc(block_size);
    if (!buffer) {
        perror("Memory allocation failed");
        return 1;
    }

    int result = 0;
    ssize_t bytes_read;
    while ((bytes_read = read(pipe_fd, buffer, block_size)) != 0) {
        if (bytes_read == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("Read from pipe failed");
            result = 1;
            break;
        }
        fwrite(buffer, 1, bytes_read, stdout);
    }

    free(buffer);
    return result;
}


int main(void) {
    if (check_std_fds() != 0) {
        return ERR_STD_FD_CLOSED;
    }

    // Pipes initialization
    int par_to_ch_pipe_fd[2], ch_to_ch_pipe_fd[2], ch_to_par_pipe_fd[2];
    if (pipe(par_to_ch_pipe_fd) == -1) {
        perror("Pipe");
        exit(EXIT_FAILURE);
    }
    if (pipe(ch_to_ch_pipe_fd) == -1) {
        perror("Pipe");
        exit(EXIT_FAILURE);
    }
    if (pipe(ch_to_par_pipe_fd) == -1) {
        perror("Pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid1, pid2;
    pid1 = fork();
    if (pid1 == -1) {
        perror("Fork 1");
        exit(EXIT_FAILURE);
    }
    else if (pid1 == 0) {
        // Child1 process
        if (dup2(par_to_ch_pipe_fd[0], 0) == -1) {
            perror("Dup2");
            _exit(ERR_SETUP_FAILED);
        }
        if (dup2(ch_to_ch_pipe_fd[1], 1) == -1) {
            perror("Dup2");
            _exit(ERR_SETUP_FAILED);
        }

        close(par_to_ch_pipe_fd[0]); close(par_to_ch_pipe_fd[1]);
        close(ch_to_ch_pipe_fd[0]); close(ch_to_ch_pipe_fd[1]);
        close(ch_to_par_pipe_fd[0]); close(ch_to_par_pipe_fd[1]);

        char *lower_name = "lower";
        char *lower_path = "./lower";
        char *args[] = {lower_name, NULL};
        execv(lower_path, args);

        perror("Execv failed");
        _exit(ERR_EXEC_FAILED);
    }

    pid2 = fork();
    if (pid2 == -1) {
        perror("Fork 2");
        close(par_to_ch_pipe_fd[0]); close(par_to_ch_pipe_fd[1]);
        close(ch_to_ch_pipe_fd[0]); close(ch_to_ch_pipe_fd[1]);
        close(ch_to_par_pipe_fd[0]); close(ch_to_par_pipe_fd[1]);
        wait_for_child_status(pid1, "Child 1 (lower)");
        exit(EXIT_FAILURE);
    }
    else if (pid2 == 0) {
        // Child2 process
        if (dup2(ch_to_ch_pipe_fd[0], 0) == -1) {
            perror("Dup2");
            _exit(ERR_SETUP_FAILED);
        }
        if (dup2(ch_to_par_pipe_fd[1], 1) == -1) {
            perror("Dup2");
            _exit(ERR_SETUP_FAILED);
        }

        close(par_to_ch_pipe_fd[0]); close(par_to_ch_pipe_fd[1]);
        close(ch_to_ch_pipe_fd[0]); close(ch_to_ch_pipe_fd[1]);
        close(ch_to_par_pipe_fd[0]); close(ch_to_par_pipe_fd[1]);

        char *imploder_name = "imploder";
        char *imploder_path = "./imploder";
        char *args[] = {imploder_name, NULL};
        execv(imploder_path, args);

        perror("Execv failed");
        _exit(ERR_EXEC_FAILED);
    }

    struct sigaction sa = {0};
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        perror("sigaction");
    }

    // close explicit descriptors
    close(par_to_ch_pipe_fd[0]);
    close(ch_to_ch_pipe_fd[0]); close(ch_to_ch_pipe_fd[1]);
    close(ch_to_par_pipe_fd[1]);

    // parent process continuation
    fprintf(stderr, "Input desired string: ");
    int result = 0;

    size_t input_length = 0;
    char *input_string = dynamic_input_reading(stdin, &input_length);
    if (!input_string) {
        fprintf(stderr, "Memory allocation for input failed.\n");
        result |= ERR_FLAG_MEM;
    }

    if (input_string) {
        if (write_to_pipe(par_to_ch_pipe_fd[1], input_string, input_length) != 0) {
            if (errno == EPIPE) {
                fprintf(stderr, "Child 1 (lower) stopped accepting input.\n");
            } else {
                perror("Write to child 1 failed");
            }
            result |= ERR_FLAG_WRITE;
        }
    }
    close(par_to_ch_pipe_fd[1]);

    size_t block_size = 1024;
    if (print_from_pipe(ch_to_par_pipe_fd[0], block_size)) {
        result |= ERR_FLAG_IO;
    }
    close(ch_to_par_pipe_fd[0]);

    if (wait_for_child_status(pid1, "Child 1 (lower)")) {
        result |= ERR_FLAG_CHILD1;
    }
    if (wait_for_child_status(pid2, "Child 2 (imploder)")) {
        result |= ERR_FLAG_CHILD2;
    }

    if (ferror(stdin)) {
        fprintf(stderr, "Error reading from stdin.\n");
        result |= ERR_FLAG_IO;
    }
    if (ferror(stdout)) {
        fprintf(stderr, "Error writing to stdout.\n");
        result |= ERR_FLAG_IO;
    }

    free(input_string);
    return result;
}