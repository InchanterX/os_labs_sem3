#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "writer.h"

#define BLOCK_SIZE 1024

int main(void) {
    char mem[BLOCK_SIZE];
    ssize_t bytes_read;
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

        for (ssize_t i = 0; i < bytes_read; i++) {
            if (mem[i] >= 'A' && mem[i] <= 'Z') {
                mem[i] += 32;
            }
        }
        if (write_to_pipe(STDOUT_FILENO, mem, bytes_read)) {
            perror("Write failed");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}