#include <stdio.h>
#include <stdlib.h>

void envVar() {
    char* envVar = getenv("HELLO_VAR");
    if(!envVar) {
        printf("environment var must be defined\n");
    }
    printf("%s\n", envVar);

    putenv("HELLO_VAR=new hello");

    envVar = getenv("HELLO_VAR");

    printf("%s\n", envVar);
}

int main() {
    envVar();
    return 0;
}
