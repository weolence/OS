#include "list.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

Storage *storage_create(int capacity) {
    Storage *storage = (Storage*) malloc(sizeof(Storage));
    if (!storage) {
        perror("malloc() failed to allocate memory for a storage");
        exit(ENOMEM);
    }

    storage->capacity = capacity;
    storage->first = NULL;
    pthread_rwlock_init(&storage->sync, NULL);

    return storage;
}

void storage_destroy(Storage *storage) {
    if (!storage) {
        errno = EINVAL;
        exit(EINVAL);
    }

    pthread_rwlock_wrlock(&storage->sync);
    Node *curr = storage->first;
    while (curr) {
        Node *next = curr->next;
        pthread_rwlock_destroy(&curr->sync);
        free(curr);
        curr = next;
    }
    pthread_rwlock_unlock(&storage->sync);

    pthread_rwlock_destroy(&storage->sync);

    free(storage);
}

static Node *create_node(const char *value) {
    Node *new_node = malloc(sizeof(Node));
    if (!new_node) {
        perror("malloc() failed to allocate memory for a new node");
        exit(ENOMEM);
    }

    strcpy(new_node->value, value);
    pthread_rwlock_init(&new_node->sync, NULL);
    new_node->next = NULL;

    return new_node;
}

void storage_add(Storage *storage, const char *value) {
    Node *new_node = create_node(value);

    if (storage->first != NULL) {
        Node *node = storage->first;
        while (node->next != NULL) {
            node = node->next;
        }
        node->next = new_node;
    } else {
        storage->first = new_node;
    }
}

void storage_fill(Storage *storage) {
    for (int i = 0; i < storage->capacity; ++i) {
        int len = rand() % 99 + 1;  // Длина от 1 до 99
        char value[100];
        for (int j = 0; j < len; j++) {
            value[j] = 'a' + rand() % 26;
        }
        value[len] = '\0';
        storage_add(storage, value);
    }
}