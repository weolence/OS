#include <stdio.h>
#include <unistd.h>

int main(void) {
    pid_t pid = fork();

    if(!pid) {
        printf("child: pid is %d, ppid is %d\n", getpid(), getppid());
        sleep(20);
        printf("child: ppid after end of parent process is %d\n", getppid());
    } else {
        printf("parent: pid is %d\n", getpid());
    }

    return 0;
}
