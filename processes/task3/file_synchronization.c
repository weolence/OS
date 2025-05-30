#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/stat.h>
#include <wait.h>

#define STACK_SIZE 1024 * 1024

void func(int counter) {
    if(counter >= 10) {
        return;
    }
    char str[16];
    snprintf(str, sizeof(str), "hello world %d", counter);
    func(counter + 1);
}

int enter(void* arg) {
    printf("enterance of child\n");
    func(0);
    printf("end of child\n");
    return 0;
}

int main(void) {
    int fd = open("stack.txt", O_RDWR | O_TRUNC, 0666);
    if(fd == -1) {
        printf("file error\n");
        return 1;
    }

    if(ftruncate(fd, STACK_SIZE) == -1) {
        printf("truncate error\n");
        close(fd);
        return 1;
    }

    void* ptr = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FILE, fd, 0);
    if(ptr == MAP_FAILED) {
        printf("mmap error\n");
        return 1;
    }
 
    pid_t pid = clone(enter, ptr + STACK_SIZE, CLONE_VM | CLONE_FILES | CLONE_SIGHAND | CLONE_FS, NULL);
   
    waitpid(pid, NULL, 0);  

    return 0;
}
