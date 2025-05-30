#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("parent: pid is %d\n", getpid());

    pid_t pid = fork();

    if(!pid) {
        printf("child: pid = %d, ppid is %d\n", getpid(), getppid());
        _exit(5);
    } else {
        sleep(30);
    }
    // в этот раз не вызываем waitpid и до конца исполнения родительского
    // процесса подпроцесс остаётся в состоянии зомби(30 сек) и ждёт пока
    // родительский процесс обработает результат его исполнения(этого не
    // происходит)
    //
    // состояние зомби создано для обработки результатов исполнения процесса

    return 0;
}
