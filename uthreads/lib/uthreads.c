#include "uthreads.h"

#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <errno.h>

#define UTHREADS_LIMIT 128
#define STACK_SIZE (1024 * 1024)

enum {
    FALSE = 0,
    TRUE = 1,
};

/* ----- uthreads storage (queue) ----- */
static size_t head_index = 0;
static size_t curr_index = 0;
static uthread_t *queue[UTHREADS_LIMIT];

static int queue_push(uthread_t *thread) {
    if (head_index >= UTHREADS_LIMIT || !thread) {
        return EXIT_FAILURE;
    }
    queue[head_index++] = thread;
    return EXIT_SUCCESS;
}

static uthread_t* queue_peek_next_valid(void) {
    if (head_index == 0) return NULL;
    size_t scanned = 0;
    while (scanned < head_index) {
        curr_index = (curr_index + 1) % head_index;
        uthread_t *t = queue[curr_index];
        scanned++;
        if (t && !t->finished) return t;
    }
    return NULL;
}

static void queue_clear(void) {
    head_index = 0;
    curr_index = 0;
    for (size_t i = 0; i < UTHREADS_LIMIT; i++) {
        queue[i] = NULL;
    }
}

static int queue_is_full(void) {
    return head_index >= UTHREADS_LIMIT;
}

/* ----- uthreads realization ----- */
static ucontext_t main_context, exit_context;
static void *exit_stack = NULL;
static uthread_t *curr_thread = NULL;
static int uthreads_initialized = FALSE;

void thread_routine(void) {
    if (curr_thread && curr_thread->start_routine) {
        uthread_exit(curr_thread->start_routine(curr_thread->arg));
    } else {
        uthread_exit(NULL);
    }
}

void exit_routine(void) {
    if (curr_thread) {
        if (curr_thread->stack) {
            free(curr_thread->stack);
            curr_thread->stack = NULL;
        }
        curr_thread->finished = 1;
    }
    setcontext(&main_context);
}

int uthreads_init(void) {
    if (uthreads_initialized) {
        return EXIT_SUCCESS;
    }

    if (getcontext(&exit_context) == -1) {
        return EXIT_FAILURE;
    }

    exit_stack = malloc(STACK_SIZE);
    if (!exit_stack) {
        return EXIT_FAILURE;
    }

    exit_context.uc_stack.ss_sp = exit_stack;
    exit_context.uc_stack.ss_size = STACK_SIZE;
    exit_context.uc_stack.ss_flags = 0;
    exit_context.uc_link = &main_context;
    makecontext(&exit_context, (void (*)(void))exit_routine, 0);

    queue_clear();
    curr_thread = NULL;
    uthreads_initialized = TRUE;

    return EXIT_SUCCESS;
}

void uthread_system_shutdown(void) {
    if (!uthreads_initialized) return;

    if (exit_stack) {
        free(exit_stack);
        exit_stack = NULL;
    }

    uthreads_initialized = 0;
}

int uthread_create(uthread_t *thread, void *(*start_routine)(void *), void *arg) {
    if (!uthreads_initialized || !thread || !start_routine) {
        errno = EINVAL;
        return EXIT_FAILURE;
    }

    if (queue_is_full()) {
        errno = EAGAIN;
        return EXIT_FAILURE;
    }

    if (getcontext(&thread->context) == -1) {
        return EXIT_FAILURE;
    }

    thread->stack = malloc(STACK_SIZE);
    if (!thread->stack) {
        return EXIT_FAILURE;
    }

    thread->context.uc_stack.ss_sp = thread->stack;
    thread->context.uc_stack.ss_size = STACK_SIZE;
    thread->context.uc_stack.ss_flags = 0;
    thread->context.uc_link = &exit_context;

    thread->start_routine = start_routine;
    thread->arg = arg;
    thread->finished = 0;
    thread->retval = NULL;

    makecontext(&thread->context, (void (*)(void))thread_routine, 0);

    queue_push(thread);

    return EXIT_SUCCESS;
}

void uthread_run(void) {
    if (!uthreads_initialized) return;
    while (TRUE) {
        uthread_t *next = queue_peek_next_valid();
        if (!next) {
            break;
        }
        curr_thread = next;
        swapcontext(&main_context, &curr_thread->context);
    }
    curr_thread = NULL;
}

void uthread_yield(void) {
    if (curr_thread && !curr_thread->finished) {
        swapcontext(&curr_thread->context, &main_context);
    }
}

void uthread_exit(void *retval) {
    if (!curr_thread) {
        setcontext(&main_context);
        return;
    }
    curr_thread->retval = retval;
    setcontext(&exit_context);
}

void *uthread_join(uthread_t *thread) {
    if (!thread) return NULL;
    while (!thread->finished) {
        uthread_yield();
    }
    return thread->retval;
}