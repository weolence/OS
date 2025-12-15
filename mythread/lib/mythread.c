#define _GNU_SOURCE

#include "mythread.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <stdatomic.h>
#include <sys/param.h>
#include <pthread.h>

#define STACK_SIZE 1024 * 1024

enum {
    FALSE = 0,
    TRUE = 1,
};

typedef struct cleanup_node {
    void (*func)(void*);
    void *arg;
    struct cleanup_node *next;
} cleanup_node;

typedef struct thread_data {
    mythread_t tid;
    void *retval;
    void *memory; // allocated for stack & guard pages
    size_t memory_size;
    atomic_int finished;
    atomic_int detached;
    atomic_int joined;
    atomic_int cancelled;
    cleanup_node *cleanup_stack;
    struct thread_data *next;
} thread_data;

static thread_data *threads_head = NULL;
static atomic_flag lock = ATOMIC_FLAG_INIT;

// mutex 
static void flag_lock() {
    while (atomic_flag_test_and_set(&lock)) { }
}

static void flag_unlock() {
    atomic_flag_clear(&lock);
}

// futex
static int futex_wait(int *addr, int expected) {
    return syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, expected, NULL, NULL, 0);
}

static int futex_wake(int *addr, int n) {
    return syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, n, NULL, NULL, 0);
}

// threads list functions
static void threads_add(thread_data *new) {
    if (!new) return;
    flag_lock();
    new->next = threads_head;
    threads_head = new;
    flag_unlock();
}

static thread_data *threads_find(mythread_t tid) {
    flag_lock();
    thread_data *result = NULL;
    thread_data *curr = threads_head;
    while (curr) {
        if (curr->tid == tid) {
            result = curr;
            break;
        }
        curr = curr->next;
    }
    flag_unlock();
    return result;
}

static void threads_remove(thread_data *tdata) {
    flag_lock();
    thread_data **curr = &threads_head;
    while (*curr) {
        if (*curr == tdata) {
            *curr = (*curr)->next;
            break;
        }
        curr = &(*curr)->next;
    }
    flag_unlock();
}

// cleanup functions
static void cleanup_thread(thread_data *tdata) {
    cleanup_node *curr = tdata->cleanup_stack;
    while (curr) {
        cleanup_node *next = curr->next;
        if (curr->func) curr->func(curr->arg);
        free(curr);
        curr = next;
    }
    tdata->cleanup_stack = NULL;
}

static void cleanup_free(thread_data *tdata) {
    cleanup_node *curr = tdata->cleanup_stack;
    while (curr) {
        cleanup_node *next = curr->next;
        free(curr);
        curr = next;
    }
}

// mythread realization
static int thread_execute(void *args) {
    if (!args) {
        perror("thread execute args");
        return EXIT_FAILURE;
    }

    void *(*routine)(void*) = ((void**)args)[0];
    void *routine_args = ((void**)args)[1];
    thread_data *tdata = (thread_data *)((void**)args)[2];
    free(args);

    tdata->tid = mythread_self();

    void *retval = routine(routine_args);

    mythread_exit(retval);

    __builtin_unreachable();
}

int mythread_create(mythread_t *thread, void *(*start_routine)(void *), void *arg) {
    if (!thread || !start_routine) {
        errno = EINVAL;
        return EXIT_FAILURE;
    }

    size_t page = sysconf(_SC_PAGESIZE);
    size_t total = STACK_SIZE + page;

    void *memory = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        perror("mmap() failured");
        return EXIT_FAILURE;
    }

    if (mprotect(memory, page, PROT_NONE) != 0) {
        perror("mprotect guard page");
        munmap(memory, total);
        return EXIT_FAILURE;
    }
    void *stack_top = (char*)memory + total;

    thread_data *tdata = malloc(sizeof(thread_data));
    if (!tdata) {
        perror("malloc thread_data");
        munmap(memory, total);
        return EXIT_FAILURE;
    }

    tdata->tid = 0;
    tdata->retval = NULL;
    tdata->memory = memory;
    tdata->memory_size = total;
    atomic_store(&tdata->finished, FALSE);
    atomic_store(&tdata->detached, FALSE);
    atomic_store(&tdata->joined, FALSE);
    atomic_store(&tdata->cancelled, FALSE);
    tdata->cleanup_stack = NULL;
    tdata->next = NULL;

    void **args = malloc(3 * sizeof(void*));
    if (!args) {
        perror("thread function args");
        free(tdata);
        munmap(memory, total);
        return EXIT_FAILURE;
    }
    args[0] = start_routine;
    args[1] = arg;
    args[2] = tdata;

    threads_add(tdata);

    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD;
    int child_tid = clone(thread_execute, stack_top, flags, args);
    if (child_tid == -1) {
        perror("clone()");
        free(args);
        threads_remove(tdata);
        free(tdata);
        munmap(memory, total);
        return EXIT_FAILURE;
    }

    tdata->tid = child_tid;
    *thread = tdata->tid;

    return EXIT_SUCCESS;
}

void mythread_exit(void *retval) {
    mythread_t tid = mythread_self();
    thread_data *tdata = threads_find(tid);
    if (!tdata) {
        syscall(SYS_exit, 0);
    }

    cleanup_thread(tdata);

    tdata->retval = retval;

    atomic_store(&tdata->finished, TRUE);
    futex_wake((int *)&tdata->finished, 1);

    if (atomic_load(&tdata->detached)) {
        void *memory = tdata->memory;
        size_t memory_size = tdata->memory_size;

        threads_remove(tdata);
        cleanup_free(tdata);
        free(tdata);
        
        __asm__ volatile (
            // munmap(memory, memory_size)
            "movq $11, %%rax\n\t"    // SYS_munmap = 11
            "movq %0, %%rdi\n\t"     // memory
            "movq %1, %%rsi\n\t"     // memory_size
            "syscall\n\t"
            
            // exit(0)
            "movq $60, %%rax\n\t"    // SYS_exit
            "xorq %%rdi, %%rdi\n\t"  // exit code 0
            "syscall\n\t"
            :
            : "r"(memory), "r"(memory_size)
            : "rax", "rdi", "rsi", "rcx", "r11"
        );
    } else {
        syscall(SYS_exit, 0);
    }
    
    __builtin_unreachable();
}

mythread_t mythread_self(void) {
    return (unsigned long)syscall(SYS_gettid);
}

int mythread_equal(mythread_t t1, mythread_t t2) {
    return t1 == t2;
}

int mythread_join(mythread_t thread, void **retval) {
    if (thread == 0) {
        perror("join invalid tid");
        errno = EINVAL;
        return -1;
    }

    mythread_t self = mythread_self();
    if (self == thread) {
        perror("self-join");
        errno = EDEADLK;
        return -1;
    }

    flag_lock();
    thread_data *curr = threads_head;
    thread_data *tdata = NULL;
    while (curr) {
        if (curr->tid == thread) {
            tdata = curr;
            break;
        }
        curr = curr->next;
    }

    if (!tdata) {
        flag_unlock();
        perror("during join thread data lost");
        errno = ESRCH;
        return -1;
    }

    if (atomic_load(&tdata->detached)) {
        flag_unlock();
        perror("during join found thread detached");
        errno = EINVAL;
        return -1;
    }

    atomic_store(&tdata->joined, TRUE);
    flag_unlock();

    while (!atomic_load(&tdata->finished)) {
        futex_wait((int *)&tdata->finished, FALSE);
    }

    if (retval) *retval = tdata->retval;

    threads_remove(tdata);

    free(tdata);

    return EXIT_SUCCESS;
}

int mythread_detach(mythread_t thread) {
    flag_lock();
    thread_data *curr = threads_head;
    thread_data *tdata = NULL;
    while (curr) {
        if (curr->tid == thread) {
            tdata = curr;
            break;
        }
        curr = curr->next;
    }
    if (!tdata) {
        flag_unlock();
        perror("during detach thread data lost");
        errno = ESRCH;
        return -1;
    }

    if (atomic_load(&tdata->detached)) {
        flag_unlock();
        perror("during detach thread have already detached");
        errno = EINVAL;
        return -1;
    }

    if (atomic_load(&tdata->joined)) {
        flag_unlock();
        perror("during detach thread have already joined");
        errno = EINVAL;
        return -1;
    }

    atomic_store(&tdata->detached, TRUE);
    flag_unlock();

    return 0;
}


int mythread_cancel(mythread_t thread) {
    thread_data *tdata = threads_find(thread);
    if (!tdata) {
        errno = ESRCH;
        return -1;
    }

    atomic_store(&tdata->cancelled, TRUE);
    
    if (mythread_self() == thread) {
        mythread_testcancel();
    }

    return EXIT_SUCCESS;
}

void mythread_testcancel(void) {
    mythread_t tid = mythread_self();
    thread_data *tdata = threads_find(tid);
    if (!tdata) return;
    if (atomic_load(&tdata->cancelled)) {
        tdata->retval = (void*)PTHREAD_CANCELED;
        atomic_store(&tdata->finished, TRUE);
        futex_wake((int *)(&tdata->finished), INT_MAX);
        mythread_exit(tdata->retval);
    }
}

void mythread_cleanup_push(void (*func)(void *), void *arg) {
    mythread_t tid = mythread_self();
    thread_data *tdata = threads_find(tid);
    if (!tdata) return;

    cleanup_node *node = malloc(sizeof(cleanup_node));
    if (!node) return;

    node->func = func;
    node->arg = arg;

    flag_lock();
    node->next = tdata->cleanup_stack;
    tdata->cleanup_stack = node;
    flag_unlock();
}

void mythread_cleanup_pop(int execute) {
    mythread_t tid = mythread_self();
    thread_data *tdata = threads_find(tid);
    if (!tdata) return;

    flag_lock();
    cleanup_node *node = tdata->cleanup_stack;
    if (!node) {
        flag_unlock();
        return;
    }
    tdata->cleanup_stack = tdata->cleanup_stack->next;
    flag_unlock();

    if (execute && node->func) node->func(node->arg);
    
    free(node);
}