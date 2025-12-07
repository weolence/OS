#pragma once

#include <unistd.h>

ssize_t uthread_read(int fd, void *buf, size_t count);
ssize_t uthread_write(int fd, const void *buf, size_t count);