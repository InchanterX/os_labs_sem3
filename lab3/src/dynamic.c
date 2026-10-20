#include <stdlib.h>
#include "dynamic.h"

dynamic_array *dynamic_create(size_t capacity) {
    if (capacity == 0) capacity = 16;

    dynamic_array *array = malloc(sizeof(*array));
    if (!array) return NULL;

    array->data = malloc(capacity * sizeof(*array->data));
    if (!array->data) {
        free(array);
        return NULL;
    }

    array->size = 0;
    array->capacity = capacity;
    return array;
}

void dynamic_destroy(dynamic_array *array) {
    if (!array) return;

    free(array->data);
    free(array);
}

int dynamic_push_back(dynamic_array *array, size_t value) {
    if (!array) return -1;

    if (array->size >= array->capacity) {
        size_t capacity = array->capacity * 2;
        size_t *new = realloc(array->data, capacity * sizeof(*new));
        if (!new) return -1;
        array->data = new;
        array->capacity = capacity;
    }

    array->data[array->size++] = value;
    return 0;
}
