#define _GNU_SOURCE
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>
#include <stdio.h>

#include "mutex.h"

// thread will wait while var on uaddr is val
int futex_wait(atomic_uint *uaddr, int val) {
    return syscall(SYS_futex, uaddr, FUTEX_WAIT, val, NULL, NULL, 0);
}

int futex_wake(atomic_uint *uaddr, int n) {
    return syscall(SYS_futex, uaddr, FUTEX_WAKE, n, NULL, NULL, 0);
}

void mutex_init(mutex_t *mutex) {
    if (!mutex) {
        errno = EINVAL;
        perror("mutex_init");
        return;
    }
    atomic_store(&mutex->owner_tid, 0);
}

void mutex_lock(mutex_t *mutex) {
    if (!mutex) {
        errno = EINVAL;
        perror("mutex_lock");
        return;
    }

    unsigned int self = syscall(SYS_gettid);
    if (atomic_load(&mutex->owner_tid) == self) {
        errno = EINVAL;
        perror("mutex_lock, already captured");
        return;
    }

    while (1) {
        int expected = 0;
        if (atomic_compare_exchange_strong(&mutex->owner_tid, &expected, self)) {
            return;
        }
        unsigned int val = 0;
        if ((val = atomic_load(&mutex->owner_tid))) {
            futex_wait(&mutex->owner_tid, val);
        }
    }
}

void mutex_unlock(mutex_t *mutex) {
    if (!mutex) {
        errno = EINVAL;
        perror("mutex_lock");
        return;
    }

    unsigned int self = syscall(SYS_gettid);
    if (atomic_load(&mutex->owner_tid) != self) {
        errno = EACCES;
        perror("mutex_unlock");
        return;
    }
    
    atomic_store(&mutex->owner_tid, 0);
    futex_wake(&mutex->owner_tid, 1);
}