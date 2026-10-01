#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#include "writer.h"

#define BLOCK_SIZE 1024

int main(void) {
    char mem[BLOCK_SIZE];
    ssize_t bytes_read;

    bool last_char_was_a_space = false;
    for (;;) {
        bytes_read = read(STDIN_FILENO, mem, sizeof(mem));

        if (bytes_read == 0) break;

        if (bytes_read == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("Read failed");
            exit(EXIT_FAILURE);
        }

        ssize_t write_index = 0;
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (last_char_was_a_space && mem[i] == ' ') {
                continue;
            } else if (mem[i] == ' ') {
                mem[write_index++] = mem[i];
                last_char_was_a_space = true;
            } else {
                mem[write_index++] = mem[i];
                last_char_was_a_space = false;
            }
        }

        if (write_to_pipe(STDOUT_FILENO, mem, write_index)) {
            perror("Write failed");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}