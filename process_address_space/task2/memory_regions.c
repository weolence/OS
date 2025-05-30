#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>

void segvHandler(int sig) {
    printf("SIGSEGV caught\n");
    _exit(1);
}
// после попытки прочитать в памяти не предназначенной для чтения эта функция
// будет обрабатывать ошибку сегментации, если не прекратить исполнение
// кода(убрать exit), то сообщение будет выводиться циклически т.к. после
// исключения выполнение продолжается с той же инструкции => опять получаем
// исключение

int main(void) {
    long pageSize = sysconf(_SC_PAGESIZE);
    printf("pid is %d\n", getpid());
    
    sleep(15);

    printf("mmap 10 pages to process\n");
    void* ptr = mmap(NULL, pageSize * 10, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    // появляется новая строка в pid/maps отвечающая заданным параметрам в
    // mmap, в виртуальной памяти в свою очередь выделяется затребованное
    // пространство
    //
    sleep(10);

    printf("protecting region\n");
    mprotect(ptr, pageSize * 10, PROT_NONE);

    sleep(10);

    struct sigaction sa;
    sa.sa_handler = segvHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGSEGV, &sa, NULL);
    
    printf("attempt of reading\n");
    char value = *((char*)ptr);
    printf("value is %c\n", value);
    // ещё при попытке чтения вылетает ошибка сегментации
    // программа завершается

    sleep(10);

    printf("attempt of writing\n");
    *((char*)ptr) = 'H';

    return 0;
}
