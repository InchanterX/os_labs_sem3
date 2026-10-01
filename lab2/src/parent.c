#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "writer.h"

#define ERR_EXEC_FAILED 127

char *dynamic_input_reading(FILE *std_in_stream, size_t *out_length) {
    if (!std_in_stream || !out_length) return NULL;
    size_t size = 128;
    size_t length = 0;
    char *buffer = malloc(size);
    if (!buffer) return NULL;

    int piece;
    while ((piece = fgetc(std_in_stream)) != EOF && piece != '\n') {
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

void print_from_pipe(int pipe_fd, size_t block_size) {
    char *buffer = malloc(block_size);
    if (!buffer) {
        perror("Memory allocation failed");
        return;
    }

    ssize_t bytes_read;
    while ((bytes_read = read(pipe_fd, buffer, block_size)) > 0) {
        fwrite(buffer, 1, bytes_read, stdout);
    }

    if (bytes_read == -1) {
        perror("Read from pipe failed");
    } else {
        fprintf(stdout, "\n");
    }
    free(buffer);
}


int main(void)
{
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
            _exit(EXIT_FAILURE);
        }
        if (dup2(ch_to_ch_pipe_fd[1], 1) == -1) {
            perror("Dup2");
            _exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
    }
    else if (pid2 == 0) {
        // Child2 process
        if (dup2(ch_to_ch_pipe_fd[0], 0) == -1) {
            perror("Dup2");
            _exit(EXIT_FAILURE);
        }
        if (dup2(ch_to_par_pipe_fd[1], 1) == -1) {
            perror("Dup2");
            _exit(EXIT_FAILURE);
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

    // close explicit descriptors
    close(par_to_ch_pipe_fd[0]);
    close(ch_to_ch_pipe_fd[0]); close(ch_to_ch_pipe_fd[1]);
    close(ch_to_par_pipe_fd[1]);

    // parent process continuation
    fprintf(stdout, "Input desired string: ");
    fflush(stdout);
    size_t input_length = 0;
    char *input_string = dynamic_input_reading(stdin, &input_length);
    if (!input_string) return 1;
    if(write_to_pipe(par_to_ch_pipe_fd[1], input_string, input_length)) {
        // What to do then?
    }

    close(par_to_ch_pipe_fd[1]);
    size_t block_size = 1024;
    print_from_pipe(ch_to_par_pipe_fd[0], block_size);
    close(ch_to_par_pipe_fd[0]);

    char* ch1 = "Child 1 (lower)";
    char* ch2 = "Child 2 (imploder)";
    int code1 = wait_for_child_status(pid1, ch1);
    int code2 = wait_for_child_status(pid2, ch2);
    free(input_string);
    if (code1) {
        return 1;
    } else if (code2) {
        return 2;
    } else {
        return 0;
    }
}