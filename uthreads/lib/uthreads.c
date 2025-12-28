#include "uthreads.h"
#include "uthreads_queue.h"
#include "thread_local_storage.h"

#include <ucontext.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdarg.h>

enum {
    FALSE = 0,
    TRUE = 1,
};

#define STACK_SIZE 1024 * 1024

static atomic_int uthreads_initialized = FALSE;
static atomic_int uthreads_started = FALSE;

static tlocal_t *storage_main_context = NULL;
static tlocal_t *storage_exit_context = NULL;
static tlocal_t *storage_uthreads_queue = NULL;
static tlocal_t *storage_curr_uthread = NULL;

static pthread_t *threads = NULL;
static size_t threads_size = 0;
static atomic_size_t threads_index = 0;

/* ===== utility functions ===== */

void uthread_exit_routine(void) {
    pthread_t self = pthread_self();
    uthread_t *uthread = tlocal_get(storage_curr_uthread, self);
    if (uthread) {
        // after end of uthread execution it's stack freed,
        // but struct uthread_t still alive because of retval value
        if (uthread->stack) {
            free(uthread->stack);
            uthread->stack = NULL;
        }
        uthread->finished = TRUE;
    }
    ucontext_t *main_context = tlocal_get(storage_main_context, self);
    setcontext(main_context);
}

void uthread_routine(void) {
    pthread_t self = pthread_self();
    uthread_t *uthread = tlocal_get(storage_curr_uthread, self);
    if (uthread && uthread->start_routine) {
        uthread_exit(uthread->start_routine(uthread->arg));
    } else {
        uthread_exit(NULL);
    }
}

void *pthread_routine(void *arg) {
    uthreads_queue_t *uthreads_queue = uthreads_queue_create();
    if (!uthreads_queue) {
        errno = ENOMEM;
        perror("pthread_routine");
        return NULL;
    }

    ucontext_t *main_context = calloc(1, sizeof(ucontext_t));
    if (!main_context) {
        uthreads_queue_destroy(uthreads_queue);
        errno = ENOMEM;
        perror("pthread_routine");
        return NULL;
    }

    if (getcontext(main_context) == -1) {
        uthreads_queue_destroy(uthreads_queue);
        free(main_context);
        errno = ENODATA;
        perror("pthread_routine");
        return NULL;
    }

    ucontext_t *exit_context = calloc(1, sizeof(ucontext_t));
    if (!exit_context) {
        uthreads_queue_destroy(uthreads_queue);
        free(main_context);
        errno = ENOMEM;
        perror("pthread_routine");
        return NULL;
    }
    
    if (getcontext(exit_context) == -1) {
        uthreads_queue_destroy(uthreads_queue);
        free(main_context);
        free(exit_context);
        errno = ENODATA;
        perror("pthread_routine");
        return NULL;
    }

    void *exit_stack = malloc(STACK_SIZE);
    if (!exit_stack) {
        uthreads_queue_destroy(uthreads_queue);
        free(main_context);
        free(exit_context);
        errno = ENOMEM;
        perror("pthread_routine");
        return NULL;
    }

    exit_context->uc_stack.ss_sp = exit_stack;
    exit_context->uc_stack.ss_size = STACK_SIZE;
    exit_context->uc_stack.ss_flags = 0;
    exit_context->uc_link = main_context;
    makecontext(exit_context, uthread_exit_routine, 0);

    pthread_t self = pthread_self();

    tlocal_set(storage_uthreads_queue, self, uthreads_queue);
    tlocal_set(storage_exit_context, self, exit_context);
    tlocal_set(storage_main_context, self, main_context);

    while (!atomic_load(&uthreads_started)) { }

    while (TRUE) {
        pthread_t self = pthread_self();

        uthread_t *curr_uthread = tlocal_get(storage_curr_uthread, self);

        // put executed earlier uthread.
        // if this uthread finished, it will not get into uthreads_queue.
        // struct of this thread will be freed only after joining it
        if (curr_uthread && !curr_uthread->finished) {
            uthreads_queue_add(uthreads_queue, curr_uthread);
        }

        // get next uthread from queue
        uthread_t *next_uthread = uthreads_queue_get(uthreads_queue);
        if (!next_uthread) {
            break; // no uthreads for executing
        }

        // set next uthread as current
        tlocal_set(storage_curr_uthread, self, next_uthread);

        // swap to next uthread context
        ucontext_t *main_context = tlocal_get(storage_main_context, self);
        swapcontext(main_context, &next_uthread->context);
    }

    return NULL;
}

/* ===== end of utility functions ===== */

int uthreads_init(size_t kernel_threads_num) {
    if (atomic_load(&uthreads_initialized)) {
        return EXIT_SUCCESS;
    }

    threads_size = kernel_threads_num;
    threads = calloc(threads_size, sizeof(pthread_t));
    if (!threads) {
        errno = ENOMEM;
        perror("uthreads_init");
        return EXIT_FAILURE;
    }

    // creating tls of queues (every thread will have it own queue)
    storage_uthreads_queue = tlocal_create(threads_size);
    if (!storage_uthreads_queue) {
        free(threads);
        errno = ENOMEM;
        perror("uthreads_init");
        return EXIT_FAILURE;
    }

    // creating tls of main contexts (every thread will have it own main context)
    storage_main_context = tlocal_create(threads_size);
    if (!storage_main_context) {
        free(threads);
        tlocal_destroy(storage_uthreads_queue);
        errno = ENOMEM;
        perror("uthreads_init");
        return EXIT_FAILURE;
    }

    // creating tls of exit contexts (every thread will have it own exit context)
    storage_exit_context = tlocal_create(threads_size);
    if (!storage_exit_context) {
        free(threads);
        tlocal_destroy(storage_uthreads_queue);
        tlocal_destroy(storage_main_context);
        errno = ENOMEM;
        perror("uthreads_init");
        return EXIT_FAILURE;
    }

    storage_curr_uthread = tlocal_create(threads_size);
    if (!storage_curr_uthread) {
        free(threads);
        tlocal_destroy(storage_uthreads_queue);
        tlocal_destroy(storage_main_context);
        tlocal_destroy(storage_exit_context);
        errno = ENOMEM;
        perror("uthreads_init");
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < threads_size; i++) {
        pthread_t thread;
        pthread_create(&thread, NULL, pthread_routine, NULL);
        threads[i] = thread;
    }

    atomic_store(&uthreads_initialized, TRUE);

    return EXIT_SUCCESS;
}

int uthread_create(uthread_t *uthread, void *(*start_routine)(void *), void *arg, ...) {
    if (!atomic_load(&uthreads_initialized) || !uthread || !start_routine) {
        errno = EINVAL;
        perror("uthread_create");
        return EXIT_FAILURE;
    }

    va_list args;
    va_start(args, arg);
    size_t *opt_index_ptr = va_arg(args, size_t *);
    size_t index = 0;
    
    if (opt_index_ptr) {
        if (*opt_index_ptr >= threads_size) {
            errno = EINVAL;
            perror("uthread_create");
            return EXIT_FAILURE;
        }
        index = *opt_index_ptr;
    } else {
        index = atomic_load(&threads_index);
        atomic_store(&threads_index, (index + 1) % threads_size);
    }
    pthread_t pthread = threads[index]; // thread will receive task through queue

    va_end(args);

    if (getcontext(&uthread->context) == -1) {
        errno = ENODATA;
        perror("uthread_create");
        return EXIT_FAILURE;
    }

    uthread->stack = malloc(STACK_SIZE);
    if (!uthread->stack) {
        errno = ENOMEM;
        perror("uthread_create");
        return EXIT_FAILURE;
    }

    // have to wait until pthread_routine initialize uthreads_queue for thread
    uthreads_queue_t *uthreads_queue = NULL;
    while (!uthreads_queue) {
        uthreads_queue = tlocal_get(storage_uthreads_queue, pthread);
    }

    // have to wait until pthread_routine initialize exit_context for thread
    ucontext_t *exit_context = NULL;
    while (!exit_context) {
        exit_context = tlocal_get(storage_exit_context, pthread);
    }

    uthread->context.uc_stack.ss_sp = uthread->stack;
    uthread->context.uc_stack.ss_size = STACK_SIZE;
    uthread->context.uc_stack.ss_flags = 0;
    uthread->context.uc_link = exit_context;

    uthread->start_routine = start_routine;
    uthread->arg = arg;
    uthread->finished = 0;
    uthread->retval = NULL;

    makecontext(&uthread->context, (void (*)(void))uthread_routine, 0);

    uthreads_queue_add(uthreads_queue, uthread);

    return EXIT_SUCCESS;
}

void uthreads_run(void) {
    if (!atomic_load(&uthreads_initialized) || atomic_load(&uthreads_started)) {
        return;
    }
    atomic_store(&uthreads_started, TRUE);
}

void uthread_yield(void) {
    pthread_t self = pthread_self();
    uthread_t *uthread = tlocal_get(storage_curr_uthread, self);
    if (uthread && !uthread->finished) {
        ucontext_t *main_context = tlocal_get(storage_main_context, self);
        swapcontext(&uthread->context, main_context);
    }
}

void uthread_exit(void *retval) {
    pthread_t self = pthread_self();
    uthread_t *uthread = tlocal_get(storage_curr_uthread, self);
    if (!uthread) {
        ucontext_t *main_context = tlocal_get(storage_main_context, self);
        setcontext(main_context);
    } else {
        uthread->retval = retval;
        ucontext_t *exit_context = tlocal_get(storage_exit_context, self);
        setcontext(exit_context);
    }
}

void *uthread_join(uthread_t *uthread) {
    if (!uthread || !uthread->start_routine) {
        errno = EINVAL;
        perror("uthread_join");
        return NULL;
    }
    while (!uthread->finished) {
        sched_yield();
    }
    return uthread->retval;
}

void uthreads_system_shutdown(void) {
    if (!atomic_load(&uthreads_initialized)) {
        return;
    }

    for(size_t i = 0; i < threads_size; i++) {
        pthread_t pthread = threads[i];
        pthread_join(pthread, NULL);

        ucontext_t *main_context = tlocal_remove(storage_main_context, pthread);
        if (main_context) {
            free(main_context);
        }

        ucontext_t *exit_context = tlocal_remove(storage_exit_context, pthread);
        if (exit_context) {
            if (exit_context->uc_stack.ss_sp) {
                free(exit_context->uc_stack.ss_sp);
                exit_context->uc_stack.ss_sp = NULL;
            }
            free(exit_context);
        }

        uthreads_queue_t *uthreads_queue = tlocal_remove(storage_uthreads_queue, pthread);
        uthreads_queue_destroy(uthreads_queue);

        uthread_t *curr_uthread = tlocal_remove(storage_curr_uthread, pthread);
        if(curr_uthread) {
            if (curr_uthread->stack) {
                free(curr_uthread->stack);
                curr_uthread->stack = NULL;
            }
        }
    }

    tlocal_destroy(storage_main_context);
    tlocal_destroy(storage_exit_context);
    tlocal_destroy(storage_uthreads_queue);
    tlocal_destroy(storage_curr_uthread);

    free(threads);
    threads_size = 0;
    atomic_store(&threads_index, 0);

    atomic_store(&uthreads_started, FALSE);
    atomic_store(&uthreads_initialized, FALSE);
}