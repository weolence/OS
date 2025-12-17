#include <errno.h>
#include <stdio.h>

#include "spinlock.h"

#define RELEASED 0
#define CAPTURED 1

void spin_init(spinlock_t *spinlock) {
    if (!spinlock) {
        errno = EINVAL;
        perror("mutex_init");
        return;
    }
    spinlock->lock = RELEASED;
}

void spin_lock(spinlock_t *spinlock) {
    while (1) {
        int expected = RELEASED;
		if (atomic_compare_exchange_strong(&spinlock->lock, &expected, CAPTURED)) {
            return;
		}
	}
}

void spin_unlock(spinlock_t *spinlock) {
    atomic_store(&spinlock->lock, RELEASED);
}