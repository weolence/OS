#pragma once

#include <pthread.h>

#define STRING_LENGTH_LIMIT 100

typedef struct _Node {
    char value[STRING_LENGTH_LIMIT];
    struct _Node* next;
    pthread_spinlock_t sync;
} Node;

typedef struct _Storage {
    Node *first;
    int capacity;
    pthread_spinlock_t sync;
} Storage;

typedef enum _Thread_type {
    ASC,
    EQUAL,
    DESC,
} Thread_type;

typedef struct _Thread_data {
    Storage *storage;
    Thread_type type;
} Thread_data;

Storage *storage_create(int capacity);
void storage_destroy(Storage *storage);
void storage_add(Storage *storage, const char *value);
void storage_fill(Storage *storage);