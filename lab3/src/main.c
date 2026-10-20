#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "dynamic.h"

typedef enum {
    STATUS_OK = 0,
    STATUS_BAD_ARGS,
    STATUS_NO_MEMORY,
    STATUS_IO_ERROR,
    STATUS_THREAD_ERROR,
    STATUS_TIME_ERROR
} status_t;

typedef struct {
    const char *text;
    const char *pattern;
    size_t pattern_length;
    size_t from;
    size_t to;
    dynamic_array *positions;
    int failed;
} task_t;

char *input_data(FILE *std_in_stream, size_t *out_length) {
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

    if (ferror(std_in_stream)) {
        free(buffer);
        return NULL;
    }

    if (length > 0 && buffer[length - 1] == '\r') --length;

    buffer[length] = '\0';
    *out_length = length;
    return buffer;
}

static int is_equal(const char *text, const char *pattern, size_t pattern_length) {
    for (size_t i = 0; i < pattern_length; ++i) {
        if (text[i] != pattern[i]) return 0;
    }
    return 1;
}

void *worker(void *arg) {
    task_t *task = arg;
    for (size_t i = task->from; i < task->to; ++i) {
        if (is_equal(task->text + i, task->pattern, task->pattern_length)) {
            if (dynamic_push_back(task->positions, i) != 0) {
                task->failed = 1;
                break;
            }
        }
    }
    return NULL;
}

static status_t parse_ll(const char *str, long long min, long long max, long long *out) {
    char *end = NULL;

    errno = 0;
    long long value = strtoll(str, &end, 10);

    if (end == str)
    {
        return STATUS_BAD_ARGS;
    }
    if (*end != '\0')
    {
        return STATUS_BAD_ARGS;
    }
    if (errno == ERANGE)
    {
        return STATUS_BAD_ARGS;
    }
    if (value < min || value > max)
    {
        return STATUS_BAD_ARGS;
    }
    *out = value;
    return STATUS_OK;
}

static void print_usage(const char *program_name)
{
    fprintf(stderr, "Usage: %s -t <threads>\n", program_name);
}

static double elapsed_seconds(const struct timespec *start, const struct timespec *end) {
    return (double)(end->tv_sec - start->tv_sec) + (double)(end->tv_nsec - start->tv_nsec) / 1e9;
}

int main(int argc, char *argv[]) {
    long long threads_count = 0;
    int opt;

    while ((opt = getopt(argc, argv, "t:h")) != -1) {
        switch (opt)
        {
        case 't':
            if (parse_ll(optarg, 1, 1024, &threads_count) != STATUS_OK)
            {
                fprintf(stderr, "Invalid thread count: '%s' (expected 1..1024).\n", optarg);
                return STATUS_BAD_ARGS;
            }
            break;
        case 'h':
            print_usage(argv[0]);
            return STATUS_OK;
        default:
            print_usage(argv[0]);
            return STATUS_BAD_ARGS;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "Unexpected argument: '%s'.\n", argv[optind]);
        print_usage(argv[0]);
        return STATUS_BAD_ARGS;
    }
    if (threads_count == 0) {
        print_usage(argv[0]);
        return STATUS_BAD_ARGS;
    }

    status_t status = STATUS_OK;
    char *input_string = NULL;
    char *search_string = NULL;
    pthread_t *threads = NULL;
    task_t *tasks = NULL;
    long long created = 0;

    fprintf(stderr, "Input string to search in: ");
    size_t input_length = 0;
    input_string = input_data(stdin, &input_length);
    if (!input_string) {
        perror("Reading input string failed");
        status = STATUS_IO_ERROR;
        goto cleanup;
    }

    fprintf(stderr, "Input string to search for: ");
    size_t search_length = 0;
    search_string = input_data(stdin, &search_length);
    if (!search_string) {
        perror("Reading search string failed");
        status = STATUS_IO_ERROR;
        goto cleanup;
    }

    if (search_length == 0) {
        fprintf(stderr, "Search string must not be empty.\n");
        status = STATUS_BAD_ARGS;
        goto cleanup;
    }
    if (search_length > input_length) {
        printf("No occurrences found.\n");
        goto cleanup;
    }

    size_t positions_count = input_length - search_length + 1;
    if ((size_t)threads_count > positions_count) threads_count = (long long)positions_count;

    threads = malloc((size_t)threads_count * sizeof *threads);
    if (!threads) {
        fprintf(stderr, "Memory allocation for threads failed.\n");
        status = STATUS_NO_MEMORY;
        goto cleanup;
    }

    tasks = calloc((size_t)threads_count, sizeof *tasks);
    if (!tasks) {
        fprintf(stderr, "Memory allocation for tasks failed.\n");
        status = STATUS_NO_MEMORY;
        goto cleanup;
    }

    for (long long i = 0; i < threads_count; ++i) {
        tasks[i].positions = dynamic_create(0);
        if (!tasks[i].positions) {
            fprintf(stderr, "Memory allocation for positions failed.\n");
            status = STATUS_NO_MEMORY;
            goto cleanup;
        }
    }

    struct timespec t_start, t_end;
    if (clock_gettime(CLOCK_MONOTONIC, &t_start) == -1) {
        perror("clock_gettime failed");
        status = STATUS_TIME_ERROR;
        goto cleanup;
    }

    size_t chunk = positions_count / (size_t)threads_count;
    size_t remainder = positions_count % (size_t)threads_count;
    size_t position = 0;

    for (long long i = 0; i < threads_count; ++i) {
        size_t size = chunk + ((size_t)i < remainder ? 1 : 0);

        tasks[i].text = input_string;
        tasks[i].pattern = search_string;
        tasks[i].pattern_length = search_length;
        tasks[i].from = position;
        tasks[i].to = position + size;
        tasks[i].failed = 0;
        position += size;

        int rc = pthread_create(&threads[i], NULL, worker, &tasks[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create failed for thread %lld: %s.\n", i, strerror(rc));
            status = STATUS_THREAD_ERROR;
            break;
        }
        ++created;
    }

    for (long long i = 0; i < created; ++i) {
        int rc = pthread_join(threads[i], NULL);
        if (rc != 0) {
            fprintf(stderr, "pthread_join failed for thread %lld: %s.\n", i, strerror(rc));
            status = STATUS_THREAD_ERROR;
            continue;
        }
        if (tasks[i].failed) {
            fprintf(stderr, "Memory allocation for positions failed in thread %lld.\n", i);
            status = STATUS_NO_MEMORY;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &t_end) == -1) {
        perror("clock_gettime failed");
        status = STATUS_TIME_ERROR;
    }

    if (status == STATUS_OK) {
        size_t total = 0;
        for (long long i = 0; i < threads_count; ++i) {
            for (size_t j = 0; j < tasks[i].positions->size; ++j) {
                printf(total == 0 ? "%zu" : " %zu", tasks[i].positions->data[j]);
                ++total;
            }
        }
        if (total == 0) {
            printf("No occurrences found.\n");
        } else {
            printf("\n");
        }
        printf("Occurrences: %zu\n", total);
        printf("Threads: %lld, time: %.3f s\n", threads_count, elapsed_seconds(&t_start, &t_end));
    }

cleanup:
    if (tasks) {
        for (long long i = 0; i < threads_count; ++i) {
            dynamic_destroy(tasks[i].positions);
        }
    }
    free(tasks);
    free(threads);
    free(search_string);
    free(input_string);

    if (fflush(stdout) == EOF || ferror(stdout)) {
        fprintf(stderr, "Error writing to stdout.\n");
        if (status == STATUS_OK) status = STATUS_IO_ERROR;
    }

    return status;
}
