#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(void) {
    pid_t pid = getpid();
    printf("pid is %d\n", (int)pid);

    while(1) {
        malloc(4096 * sizeof(int));    
        sleep(2);
    }

    return 0;
}
// пространство выделенное под кучу как и в случае со стеком начинает расти в
// сторону больших адресов пока не достигнет некоего установленного системой
// придела
