#include "uthreads.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

static void *println_test(void *arg) {
    int id = (int)(long)arg;
    for (int i = 0; i < 3; i++) {
        printf("[pthread %u][uthread %d] i = %d\n", pthread_self(), id, i);
        uthread_yield();
    }
    sleep(3);
    return (void *)(long)(id * 10);
}

int main(void) {
    if (uthreads_init(2) != EXIT_SUCCESS) {
        fprintf(stderr, "uthreads_init failed\n");
        return 1;
    }

    size_t queue0 = 0, queue1 = 1;
    uthread_t println_uth1, println_uth2;
    uthread_create(&println_uth1, println_test, (void *)(long)1, queue0);
    uthread_create(&println_uth2, println_test, (void *)(long)2, queue1);

    uthreads_run();
  
    long println_uth1_res = (long)uthread_join(&println_uth1);
    long println_uth2_res = (long)uthread_join(&println_uth2);

    printf("\n----- TEST END -----\n");
    printf("println_test_uthread1 returned %ld\n", println_uth1_res);
    printf("println_test_uthread2 returned %ld\n", println_uth2_res);

    uthreads_system_shutdown();

    return 0;
}