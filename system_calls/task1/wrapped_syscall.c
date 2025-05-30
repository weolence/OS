#include <unistd.h>
#include <sys/syscall.h>

void wrapper(int handler, char* text, int size) {
    syscall(SYS_write, handler, text, size);
}

int main(void) {
    char text[] = "hello world";
    wrapper(1, text, sizeof(text));
    return 0;
}
