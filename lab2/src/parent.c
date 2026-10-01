#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

char *dynamic_input_reading(FILE *std_in_stream, size_t *out_length) {
    if (!std_in_stream || !out_len) return NULL;
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
    const char lower_name[6] = "lower";
    if (pid1 == -1) {
        perror("Fork 1");
        exit(EXIT_FAILURE);
    }
    else if (pid1 == 0) {
        // Child1 process
        if (dup2(par_to_ch_pipe_fd[0], 0) == -1) {
            perror("Dup2:");
            exit(EXIT_FAILURE);
        }
        if (dup2(ch_to_ch_pipe_fd[1], 1) == -1) {
            perror("Dup2:");
            exit(EXIT_FAILURE);
        }

        close(par_to_ch_pipe_fd[0]); close(par_to_ch_pipe_fd[1]);
        close(ch_to_ch_pipe_fd[0]); close(ch_to_ch_pipe_fd[1]);
        close(ch_to_par_pipe_fd[0]); close(ch_to_par_pipe_fd[1]);

        char *lower_path = "./lower";
        char *args[] = {lower_path, NULL};
        if(execv(*imploder_name, args) == -1) perror(EXIT);

        perror("Execv failed");
        _exit(ERR_EXEC_FAILED);
    } else {
        int status;
        if (waitpid(pid1, &status, 0) == -1) {
            perror("Waipid failed");
            return -1;
        }

        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);

            if (exit_code == ERR_EXEC_FAILED) {
                printf("Child was unable was unable to execute exec.");
            } else if (exit_code != 0) {
                printf("Child started but finished with a error.");
            } else {
                printf("Child failed with unexpected error.");
            }
        } else if (WIFSIGNALED(status)) {
            printf("Child process was killed with a signal %d.\n", WTERMSIG(status));
        }
    }

    pid2 = fork();
    const char imploder_name[9] = "imploder";
    if (pid2 == -1) {
        perror("Fork 2");
        exit(EXIT_FAILURE);
    }
    else if (pid2 == 0) {
        // Child2 process
        if (dup2(ch_to_ch_pipe_fd[0], 0) == -1) {
            perror("Dup2:");
            exit(EXIT_FAILURE);
        }
        if (dup2(ch_to_par_pipe_fd[1], 1) == -1) {
            perror("Dup2:")
            exit(EXIT_FAILURE);
        }

        close(par_to_ch_pipe_fd[0]); close(par_to_ch_pipe_fd[1]);
        close(ch_to_ch_pipe_fd[0]); close(ch_to_ch_pipe_fd[1]);
        close(ch_to_par_pipe_fd[0]); close(ch_to_par_pipe_fd[1]);

        char *imploder_path = "./imploder";
        char *args[] = {imploder_path, NULL};
        if(execv(*imploder_name, args) == -1) perror(EXIT);

        perror("Execv failed");
        _exit(ERR_EXEC_FAILED);
    }

    // close explicit descriptors
    close(par_to_ch_pipe_fd[0]);
    close(ch_to_ch_pipe_fd[0]); close(ch_to_ch_pipe_fd[1]);
    close(ch_to_par_pipe_fd[1]);

    // parent process continuation
    printf("Input desired string: ");
    size_t input_length = 0;
    char *input_string = dynamic_input_reading(stdin, &input_length);
    if (!input_string) return 1;
    write(par_to_ch_pipe_fd[1], &input_string, input_length);
    close(par_to_ch_pipe_fd[1]); close(ch_to_par_pipe_fd[0]);
    free(input_string);
    return 0;
}