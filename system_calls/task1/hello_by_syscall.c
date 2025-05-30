#include <unistd.h>
#include <sys/syscall.h>

int main(void) {
    char text[] = "hello world";
    syscall(SYS_write, 1, text, sizeof(text) - 1);
    return 0;
}
