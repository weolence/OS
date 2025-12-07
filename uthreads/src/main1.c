#include "uthreads.h"
#include "uthreads_wrappers.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static void *read_wrapper_test(void *arg) {
    int fd = *(int *)arg;
    char buf[128];
    ssize_t tot = 0;
    while (1) {
        ssize_t r = uthread_read(fd, buf, sizeof(buf) - 1);
        if (r < 0) {
            perror("uthread_read");
            break;
        }
        if (r == 0) {
            break;
        }
        buf[r] = '\0';
        tot += r;
        printf("[reader] got %zd bytes: %s", r, buf);
        uthread_yield();
    }
    close(fd);
    return (void *)(long)tot;
}

static void *write_wrapper_test(void *arg) {
    int fd = *(int *)arg;
    char buf[64];
    ssize_t total = 0;
    for (int i = 0; i < 3; i++) {
        int len = snprintf(buf, sizeof(buf), "message %d from writer\n", i);
        ssize_t w = uthread_write(fd, buf, len);
        if (w < 0) {
            perror("uthread_write");
            break;
        }
        total += w;
        printf("[writer] wrote %zd bytes\n", w);
        uthread_yield();
    }
    close(fd);
    return (void *)(long)total;
}

int main(void) {
    if (uthreads_init() != EXIT_SUCCESS) {
        fprintf(stderr, "uthreads_init failed\n");
        return 1;
    }

    int fds[2];
    if (pipe(fds) < 0) {
        perror("pipe");
        return 1;
    }

    uthread_t read_uth, write_uth;
    uthread_create(&read_uth, read_wrapper_test, &fds[0]);
    uthread_create(&write_uth, write_wrapper_test, &fds[1]);

    uthread_run();

    long w_res = (long)uthread_join(&write_uth);
    long r_res = (long)uthread_join(&read_uth);

    printf("\n----- TESTS END -----\n");
    printf("writer wrote total %ld bytes\n", w_res);
    printf("reader read  total %ld bytes\n", r_res);
  
    uthread_system_shutdown();

    return 0;
}