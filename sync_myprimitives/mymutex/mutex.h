#pragma once

#include <stdatomic.h>

typedef struct {
    atomic_uint owner_tid;
} mutex_t;

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);