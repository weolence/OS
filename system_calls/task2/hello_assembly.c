void write_hello() {
    __asm__ volatile (
        "movl $4, %%eax\n"
        "movl $1, %%ebx\n"
        "movl %0, %%ecx\n"
        "movl $12, %%edx\n"
        "int $0x80\n"
        :
        : "r" ("hello world")
        : "eax", "ebx", "ecx", "edx"
    );
}

int main(void) {
    write_hello();
    return 0;
}
