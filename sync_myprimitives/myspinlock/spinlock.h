#pragma once

#include <stdatomic.h>

typedef struct {
    atomic_int lock;
} spinlock_t;

void spin_init(spinlock_t *spinlock);
void spin_lock(spinlock_t *spinlock);
void spin_unlock(spinlock_t *spinlock);