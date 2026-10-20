#ifndef DYNAMIC_H
#define DYNAMIC_H

#include <stddef.h>

typedef struct {
    size_t *data;
    size_t size;
    size_t capacity;
} dynamic_array;

dynamic_array *dynamic_create(size_t capacity);
void dynamic_destroy(dynamic_array *array);
int dynamic_push_back(dynamic_array *array, size_t value);

#endif
