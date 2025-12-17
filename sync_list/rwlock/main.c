#include "list.h"

#include <stdatomic.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static atomic_int asc_iter_counter = 0;
static atomic_int asc_counter = 0;
static atomic_int desc_iter_counter = 0;
static atomic_int desc_counter = 0;
static atomic_int equal_iter_counter = 0;
static atomic_int equal_counter = 0;
static atomic_int swap_counter = 0;

void *monitor_routine(void *arg) {
    while (1) {
        printf("swap count: %d\n", swap_counter);
        printf("=== iterations through list ===\n");
        printf("asc: %d\ndesc: %d\nequal: %d\n", asc_iter_counter, desc_iter_counter, equal_iter_counter);
        printf("=== found elements in order ===\n");
        printf("asc: %d\ndesc: %d\nequal: %d\n\n", asc_counter, desc_counter, equal_counter);
        sleep(1);
    }
    return NULL;
}
// no sync function. expects node1->next = node2
void process_operation(Thread_type ttype, Node *node1, Node *node2) {
    switch (ttype) {
        case ASC:
            if (strlen(node1->value) < strlen(node2->value)) {
                atomic_fetch_add(&asc_counter, 1);
            }
            break;
        case EQUAL:
            if (strlen(node1->value) == strlen(node2->value)) {
                atomic_fetch_add(&equal_counter, 1);
            }
            break;
        case DESC:
            if (strlen(node1->value) > strlen(node2->value)) {
                atomic_fetch_add(&desc_counter, 1);
            }
            break;
        default:
            errno = EINVAL;
            perror("process_operation");
    }
}

void process_iter_end(Thread_type ttype) {
    switch (ttype) {
        case ASC:
            atomic_fetch_add(&asc_iter_counter, 1);
            break;
        case EQUAL:
            atomic_fetch_add(&equal_iter_counter, 1);
            break;
        case DESC:
            atomic_fetch_add(&desc_iter_counter, 1);
            break;
        default:
            errno = EINVAL;
            perror("process_iter_end");
    }
}

void *comparator_routine(void *arg) {
    Thread_data *tdata = (Thread_data *)arg;
    if (!tdata || !tdata->storage) {
        errno = EINVAL;
        exit(EXIT_FAILURE);
    }

    Storage *storage = tdata->storage;
    Thread_type ttype = tdata->type;

    while (1) {
        pthread_rwlock_rdlock(&storage->sync);

        Node *curr = storage->first;
        if (!curr) {
            pthread_rwlock_unlock(&storage->sync);
            perror("first element in storage is NULL");
            break;
        }

        pthread_rwlock_rdlock(&curr->sync);

        pthread_rwlock_unlock(&storage->sync);

        Node *next = curr->next;
        while (next) {
            pthread_rwlock_rdlock(&next->sync);

            process_operation(ttype, curr, next);

            pthread_rwlock_unlock(&curr->sync);

            curr = next;
            next = next->next;
        }

        pthread_rwlock_unlock(&curr->sync);

        process_iter_end(ttype);
    }

    return NULL;
}

// no sync function with pointers reassignment
// expects first->next = second & second->next = third
void swap_nodes(Node *first, Node **second, Node **third) {
    atomic_fetch_add(&swap_counter, 1);
    first->next = *third;
    (*second)->next = (*third)->next;
    (*third)->next = *second;
    
    // reassignment
    *second = first->next;
    *third = (*second)->next;
}

void *swapper_routine(void *arg) {
    Storage *storage = (Storage *)arg;
    if (!storage) {
        errno = EINVAL;
        exit(EXIT_FAILURE);
    }

    while (1) {
        pthread_rwlock_wrlock(&storage->sync);

        if (!storage->first || !storage->first->next) {
            pthread_rwlock_unlock(&storage->sync);
            perror("first or second element in storage is NULL");
            break;
        }

        Node *first = storage->first;
        pthread_rwlock_wrlock(&first->sync);

        Node *second = first->next;
        pthread_rwlock_wrlock(&second->sync);

        if (rand() % 2) {
            // non-function swap
            storage->first = second;
            first->next = second->next;
            second->next = first;
            
            atomic_fetch_add(&swap_counter, 1);

            // reassignment
            first = storage->first;
            second = first->next;
        }

        pthread_rwlock_unlock(&storage->sync);

        Node *third = second->next;
        while (third) {
            pthread_rwlock_wrlock(&third->sync);

            if (rand() % 2) {
                swap_nodes(first, &second, &third);
            }

            pthread_rwlock_unlock(&first->sync);

            first = second;
            second = third;
            third = third->next;
        }
        pthread_rwlock_unlock(&second->sync);
        pthread_rwlock_unlock(&first->sync);
    }

    return NULL;
}

int main(void) {
    Storage *storage = storage_create(1000);
    if (!storage) {
        perror("storage_create");
        return 1;
    }

    storage_fill(storage);
    
    pthread_t asc_thread;
    pthread_t desc_thread;
    pthread_t equal_thread;
    pthread_t swap_thread1;
    pthread_t swap_thread2;
    pthread_t swap_thread3;
    pthread_t monitor_thread;

    Thread_data asc_tdata = {
        .storage = storage,
        .type = ASC,
    };

    Thread_data desc_tdata = {
        .storage = storage,
        .type = DESC,
    };

    Thread_data equal_tdata = {
        .storage = storage,
        .type = EQUAL,
    };

    pthread_create(&monitor_thread, NULL, monitor_routine, NULL);
    pthread_create(&asc_thread, NULL, comparator_routine, &asc_tdata);
    pthread_create(&desc_thread, NULL, comparator_routine, &desc_tdata);
    pthread_create(&equal_thread, NULL, comparator_routine, &equal_tdata);
    pthread_create(&swap_thread1, NULL, swapper_routine, storage);
    pthread_create(&swap_thread2, NULL, swapper_routine, storage);
    pthread_create(&swap_thread3, NULL, swapper_routine, storage);

    pthread_join(asc_thread, NULL);
    pthread_join(desc_thread, NULL);
    pthread_join(equal_thread, NULL);
    pthread_join(swap_thread1, NULL);
    pthread_join(swap_thread2, NULL);
    pthread_join(swap_thread3, NULL);
    pthread_join(monitor_thread, NULL);

    storage_destroy(storage);

    return 0;
}