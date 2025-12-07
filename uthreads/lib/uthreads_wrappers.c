#include "uthreads_wrappers.h"
#include "uthreads.h"

#include <fcntl.h>
#include <errno.h>

static int ensure_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    if (!(flags & O_NONBLOCK) && fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }
    return 0;
}

ssize_t uthread_read(int fd, void *buf, size_t nbytes) {
    if (ensure_nonblocking(fd) < 0) {
        return -1;
    }
    while (1) {
        ssize_t n = read(fd, buf, nbytes);
        if (n >= 0) {
            return n;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            uthread_yield();
            continue;
        }
        return -1;
    }
}

ssize_t uthread_write(int fd, const void *buf, size_t nbytes) {
    if (ensure_nonblocking(fd) < 0) {
        return -1;
    }
    while (1) {
        ssize_t n = write(fd, buf, nbytes);
        if (n >= 0) {
            return n;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            uthread_yield();
            continue;
        }
        return -1;
    }
}