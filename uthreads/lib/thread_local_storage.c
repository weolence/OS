#include "thread_local_storage.h"

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>

/* ===== utility functions ===== */

// hash function
static size_t hash(pthread_t key, size_t buckets_num) {
    return ((unsigned int) key) % buckets_num;
}

// finds node, not thread-safe
static tlocal_node_t* find_node(tlocal_t *context_storage, pthread_t key) {
    size_t index = hash(key, context_storage->buckets_num);
    tlocal_node_t *curr = context_storage->buckets[index];
    while (curr) {
        if (pthread_equal(curr->key, key)) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

/* ===== end of utility functions ===== */

// creates initialized thread local storage
tlocal_t *tlocal_create(size_t buckets_num) {
    if (!buckets_num) {
        errno = EINVAL;
        perror("tlocal_create");
        return NULL;
    }

    tlocal_t *tlocal = malloc(sizeof(tlocal_t));
    if (!tlocal) {
        errno = ENOMEM;
        perror("tlocal_create");
        return NULL;
    }

    tlocal->buckets = calloc(buckets_num, sizeof(tlocal_node_t *));
    if (!tlocal->buckets) {
        free(tlocal);
        errno = ENOMEM;
        perror("tlocal_create");
        return NULL;
    }

    tlocal->buckets_num = buckets_num;
    pthread_mutex_init(&tlocal->lock, NULL);

    return tlocal;
}

// removes all nodes without removing their content
void tlocal_destroy(tlocal_t *tlocal) {
    if (!tlocal) {
        errno = EINVAL;
        perror("tlocal_destroy");
        return;
    }

    pthread_mutex_lock(&tlocal->lock);

    for (size_t i = 0; i < tlocal->buckets_num; i++) {
        tlocal_node_t *curr = tlocal->buckets[i];
        while (curr) {
            tlocal_node_t *next = curr->next;
            free(curr);
            curr = next;
        }
        tlocal->buckets[i] = NULL;
    }
    free(tlocal->buckets);

    pthread_mutex_unlock(&tlocal->lock);

    pthread_mutex_destroy(&tlocal->lock);
    free(tlocal);
}

// rewrites key-value pair if found value by given key
void tlocal_set(tlocal_t *tlocal, pthread_t key, void *value) {
    if (!tlocal || !value) {
        errno = EINVAL;
        perror("tlocal_set");
        return;
    }

    pthread_mutex_lock(&tlocal->lock);

    tlocal_node_t *curr = find_node(tlocal, key);
    if (curr) {
        curr->value = value;
        pthread_mutex_unlock(&tlocal->lock);
        return;
    }

    tlocal_node_t *node = malloc(sizeof(tlocal_node_t));
    if(!node) {
        errno = ENOMEM;
        perror("tlocal_set");
        pthread_mutex_unlock(&tlocal->lock);
        return;
    }

    node->key = key;
    node->value = value;
    node->next = NULL;

    size_t index = hash(key, tlocal->buckets_num);
    node->next = tlocal->buckets[index];
    tlocal->buckets[index] = node;

    pthread_mutex_unlock(&tlocal->lock);
}

// returns value by given key without removing node
void *tlocal_get(tlocal_t *tlocal, pthread_t key) {
    if (!tlocal) {
        errno = EINVAL;
        perror("tlocal_get");
        return NULL;
    }

    pthread_mutex_lock(&tlocal->lock);

    tlocal_node_t *node = find_node(tlocal, key);
    void *result = node ? node->value : NULL;

    pthread_mutex_unlock(&tlocal->lock);

    return result;
}

// removes node by given key & returns it's value if that node found
void *tlocal_remove(tlocal_t *tlocal, pthread_t key) {
    if (!tlocal) {
        errno = EINVAL;
        perror("tlocal_remove");
        return NULL;
    }

    pthread_mutex_lock(&tlocal->lock);
    
    size_t index = hash(key, tlocal->buckets_num);
    void *result = NULL;
    tlocal_node_t *prev = NULL;
    tlocal_node_t *curr = tlocal->buckets[index];
    while (curr) {
        if (!pthread_equal(curr->key, key)) {
            prev = curr;
            curr = curr->next;
            continue;
        }

        if (prev) {
            prev->next = curr->next;
        } else {
            tlocal->buckets[index] = curr->next;
        }
        
        result = curr->value;
        
        free(curr);

        break;
    }

    pthread_mutex_unlock(&tlocal->lock);

    return result;
}