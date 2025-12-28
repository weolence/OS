#pragma once

#include <sys/ucontext.h>

typedef struct {
    ucontext_t context;
    void *stack;
    void *(*start_routine)(void *);
    void *arg;
    void *retval;
    int finished;
} uthread_t;

int uthreads_init(size_t kernel_threads_num);
int uthread_create(uthread_t *uthread, void *(*start_routine)(void *), void *arg, ...);
void uthreads_run(void);
void uthread_yield(void);
void uthread_exit(void *retval);
void *uthread_join(uthread_t *uthread);
void uthreads_system_shutdown(void);
