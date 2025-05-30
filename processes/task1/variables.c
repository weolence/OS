#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int globVar = 7;

int main(void) {
    int locVar = 8;
    printf("globVar: value = %d , address = %p\n", globVar, &globVar);
    printf("locVar: value = %d , address = %p\n", locVar, &locVar);

    printf("pid is %d\n", (int)getpid());

    pid_t pid = fork();

    if(pid == 0) {
        printf("child: pid = %d, parent pid = %d\n", getpid(), getppid());
        
        printf("child globVar: value = %d , address = %p\n", globVar, &globVar);
        printf("child locVar: value = %d , address = %p\n", locVar, &locVar);

        globVar = 17;
        locVar = 18;

        printf("child globVar: value = %d , address = %p\n", globVar, &globVar);
        printf("child locVar: value = %d , address = %p\n", locVar, &locVar);

        _exit(5);
    } else {
        printf("parent globVar: value = %d , address = %p\n", globVar, &globVar);
        printf("parent locVar: value = %d , address = %p\n", locVar, &locVar);

        sleep(30);

        int status;
        waitpid(pid, &status, 0);

        printf("parent globVar: value = %d , address = %p\n", globVar, &globVar);
        printf("parent locVar: value = %d , address = %p\n", locVar, &locVar);
        // значения переменных ни глобальной, ни локальной не изменяются в
        // родительском процесса из-за концепции Copy-On-Write. При попытке
        // обращения к ним из разных процессв адреса в виртуальной памяти не
        // меняются, однако на деле из-за того что у каждого процесса своя
        // таблица трансляции(виртуальные адреса->RAM) они попадают на деле в различные ячейки(для
        // одного из процессов создаётся копия значения), происходит это из-за
        // того что память в maps, в которой лежат данные указана MAPS_PRIVATE.
        // Если было бы MAPS_SHARED то данные при изменении в подпроцессе
        // изменялись бы и для родительского процесса
        
        if(WIFEXITED(status)) {
            printf("parent: child process exited with %d code\n", WEXITSTATUS(status));
        } else {
            printf("parent: child process killed with %d signal\n", WTERMSIG(status));
        }

        sleep(30);
    }
    
    return 0;
}
