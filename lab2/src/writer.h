#ifndef WRITER_H
#define WRITER_H

#include <sys/types.h>

int write_to_pipe(int pipe_fd, char *mem, size_t write_index);

#endif
