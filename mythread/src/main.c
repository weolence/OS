#include "mythread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static void cleanup_free(void *arg) {
    char *buf = (char*)arg;
    printf("[cleanup] freeing buffer at %p: '%s'\n", (void*)buf, buf ? buf : "(null)");
    free(buf);
}

// function for thread exec & join, which simply allocates memory
static void *create_join_test(void *arg) {
    (void)arg;
    printf("[thread] tid=%lu started\n", mythread_self());
    char *s = strdup("result from t1");
    if (s) printf("[thread] string allocated\n");
    return s;
}

// function for detached thread test
static void *detached_thread_test(void *arg) {
    (void)arg;
    printf("[thread] tid=%lu (detached) starting, will sleep 1s and exit\n", mythread_self());
    sleep(1);
    printf("[thread] tid=%lu exiting\n", mythread_self());
    return strdup("thread done");
}

// function for cancel test with execution of cleanup stack
static void *cancelled_thread_with_cleanup_test(void *arg) {
    (void)arg;
    printf("[thread] tid=%lu started, will loop and honor cancellation\n", (unsigned long)mythread_self());

    char *buf = malloc(128);
    if (!buf) return NULL;

    snprintf(buf, 128, "thread resource (will be freed by cleanup) tid=%lu", (unsigned long)mythread_self());

    mythread_cleanup_push(cleanup_free, buf);

    for (int i = 0; i < 10; ++i) {
        printf("[thread] tid=%lu loop %d\n", mythread_self(), i);
        sleep(1);
        mythread_testcancel();
    }

    return strdup("thread done");
}

// function for cleanup_pop test with execution of received funcition
static void *cleanup_pop_with_execution_test(void *arg) {
    (void)arg;
    printf("[thread] tid=%lu started\n", (unsigned long)mythread_self());

    char *buf = malloc(64);
    if (!buf) return NULL;

    snprintf(buf, 64, "thread temporary buffer tid=%lu", (unsigned long)mythread_self());

    mythread_cleanup_push(cleanup_free, buf);

    printf("[thread] popping cleanup and executing it now\n");
    mythread_cleanup_pop(1); // cleanup_free executed here

    printf("[thread] continue and exit normally\n");

    return strdup("thread done");
}

int main(void) {
    printf("main: starting tests\n");

    // test create & join
    mythread_t t1;
    if (mythread_create(&t1, create_join_test, NULL) != 0) {
        fprintf(stderr, "failed to create t1\n");
        return 1;
    }
    printf("main: created t1=%lu\n", (unsigned long)t1);

    void *retval1 = NULL;
    if (mythread_join(t1, &retval1) != 0) {
        fprintf(stderr, "failed to join t1\n");
    } else {
        printf("main: joined t1, retval=%p '%s'\n", retval1, retval1 ? (char*)retval1 : "(null)");
        free(retval1);
    }

    printf("\n");

    // test detach
    mythread_t t2;
    if (mythread_create(&t2, detached_thread_test, NULL) != 0) {
        fprintf(stderr, "failed to create t2\n");
    } else {
        printf("main: created t2=%lu, detaching it\n", (unsigned long)t2);
        if (mythread_detach(t2) != 0) {
            perror("mythread_detach");
        }
    }
    sleep(2);

    printf("\n");

    // test cancel & cleanup
    mythread_t t3;
    if (mythread_create(&t3, cancelled_thread_with_cleanup_test, NULL) != 0) {
        fprintf(stderr, "failed to create t3\n");
    } else {
        printf("main: created t3=%lu, will cancel after 2 seconds\n", (unsigned long)t3);
        sleep(2);
        if (mythread_cancel(t3) != 0) {
            perror("mythread_cancel");
        } else {
            printf("main: cancel requested for t3\n");
        }

        void *retval3 = NULL;
        if (mythread_join(t3, &retval3) != 0) {
            perror("mythread_join t3");
        } else {
            if (retval3 == PTHREAD_CANCELED) {
                printf("main: t3 was canceled, retval == PTHREAD_CANCELED\n");
            } else {
                printf("main: t3 joined normally, retval=%p '%s'\n", retval3, retval3 ? (char*)retval3 : "(null)");
                free(retval3);
            }
        }
    }

    printf("\n");

    // test cleanup_pop with execution
    mythread_t t4;
    if (mythread_create(&t4, cleanup_pop_with_execution_test, NULL) != 0) {
        fprintf(stderr, "failed to create t4\n");
    } else {
        void *retval4 = NULL;
        if (mythread_join(t4, &retval4) != 0) {
            perror("mythread_join t4");
        } else {
            printf("main: t4 retval='%s'\n", retval4 ? (char*)retval4 : "(null)");
            free(retval4);
        }
    }

    printf("\n");

    printf("main: tests done\n");
    return 0;
}