#include <stdio.h>
#include <unistd.h>

extern char** environ;

int main(int argc, char** argv) {
    pid_t pid = getpid();
    printf("pid is %d\n", (int)pid);

    sleep(15);

    if(argc < 2) {
        char* newArgv[argc + 2];
        for(int i = 0; i < argc; ++i) {
            newArgv[i] = argv[i];
        }

        newArgv[argc] = "12";
        newArgv[argc + 1] = NULL;
    
        execve(argv[0], newArgv, environ); 
    }

    sleep(10);

    printf("hello world\n");

    return 0;
}
// при порождении подпроцесса адреса под стэк, кучу и тп выделяются заново в
// соответствии с требованиями элф-файла указанного в системном вызове как
// исполняемого
