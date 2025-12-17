#define _GNU_SOURCE
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>
#include <stdio.h>

#include "mutex.h"

#define RELEASED 0
#define CAPTURED 1

// thread will wait while var on uaddr is val
int futex_wait(atomic_int *uaddr, int val) {
    return syscall(SYS_futex, uaddr, FUTEX_WAIT, val, NULL, NULL, 0);
}

int futex_wake(atomic_int *uaddr, int n) {
    return syscall(SYS_futex, uaddr, FUTEX_WAKE, n, NULL, NULL, 0);
}

void mutex_init(mutex_t *mutex) {
    if (!mutex) {
        errno = EINVAL;
        perror("mutex_init");
        return;
    }
    mutex->lock = RELEASED;
    mutex->owner_tid = 0;
}

void mutex_lock(mutex_t *mutex) {
    int self = syscall(SYS_gettid);
    if (mutex->lock == CAPTURED && mutex->owner_tid == self) {
        errno = EINVAL;
        perror("mutex_lock, already captured");
        return;
    }
    while (1) {
        int expected = RELEASED;
        if (atomic_compare_exchange_strong(&mutex->lock, &expected, CAPTURED)) {
            mutex->owner_tid = self;
            return;
        }
        futex_wait(&mutex->lock, CAPTURED);
    }
}

void mutex_unlock(mutex_t *mutex) {
    int self = syscall(SYS_gettid);
    if (self != mutex->owner_tid) {
        errno = EACCES;
        perror("mutex_unlock");
        return;
    }
    mutex->owner_tid = 0;
    atomic_store(&mutex->lock, RELEASED);
    futex_wake(&mutex->lock, 1);
}