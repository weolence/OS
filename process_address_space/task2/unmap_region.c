#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

int main(void) {
    long pageSize = sysconf(_SC_PAGESIZE);
    printf("pid is %d\n", (int)getpid());

    sleep(15);

    printf("mmap 10 pages\n");
    void* ptr = mmap(NULL, pageSize * 10, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); 

    sleep(10);

    void* unmapPtr = ptr + 4 * pageSize;
    printf("unmap 3 pages, 4 - 6\n");
    munmap(unmapPtr, 3 * pageSize);
    // после отсоединения страниц, находящихся посередине изначально выделенное
    // место начало-конец разбивается на два подпространства с адресами начало-место
    // разбиения и место разбиения - конец
    // для наглядности без прав чтения записи
    sleep(15);

    return 0;
}
