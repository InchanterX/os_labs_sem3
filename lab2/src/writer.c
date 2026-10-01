#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "writer.h"

int write_to_pipe(int pipe_fd, char *mem, size_t write_index) {
    ssize_t bytes_written = 0, bytes_written_total = 0;
    while (write_index - bytes_written_total > 0) {
        bytes_written = write(pipe_fd, mem + bytes_written_total, write_index - bytes_written_total);

        if (bytes_written == -1) {
            if (errno == EINTR) {
                continue;
            }
            return 1;
        }

        bytes_written_total += bytes_written;
    }
    return 0;
}
