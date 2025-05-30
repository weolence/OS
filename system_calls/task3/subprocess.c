#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/user.h>
#include <sys/syscall.h>

int main() {
    pid_t forkPID = fork();

    if(forkPID == -1) {
        printf("fork failed");
        return 1;
    }

    if(forkPID == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);

        kill(getpid(), SIGSTOP);

        syscall(SYS_write, STDOUT_FILENO, "Hello world!\n", 13);

        return 0;
    } else {
        int status;
        struct user_regs_struct regs;

        while (1) {
            waitpid(forkPID, &status, 0);

            ptrace(PTRACE_GETREGS, forkPID, NULL, &regs);

            printf("Syscall: %lld\n", regs.orig_rax);

            if (WIFEXITED(status)) {
                break;
            } else {
                ptrace(PTRACE_SYSCALL, forkPID, NULL, NULL);
            }
        }
    }

    return 0;
}
