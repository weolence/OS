#include <stdio.h>
#include <unistd.h>

int* getInitVar() {
    int initVar = 2;
    printf("address of var from getInitVar is %p\n", (void*)&initVar);

    return& initVar;
}

void func() {
    int var = 12;
    printf("address of var is %p\n", (void*)&var);

    static int staticVar = 13;
    printf("address of static var is %p\n", (void*)&staticVar);
    
    const int constVar = 14;
    printf("address of const var is %p\n", (void*)&constVar);
}

int globalInitVar = 10;
int globalUninitVar;
const int constGlobVar = 11;

int main() {
    func();

    printf("address of global initialized var is %p\n", (void*)&globalInitVar);
    printf("address of global uninitialized var is %p\n", (void*)&globalUninitVar);
    printf("address of const global var is %p\n", (void*)&constGlobVar);

    int* initVar = getInitVar();
    printf("address of var from getInitVar in main is %p\n", (void*)&initVar);
    printf("%d", *initVar);
    printf("\npid is %d\n", (int)getpid());
    pause();

    return 0;
}
