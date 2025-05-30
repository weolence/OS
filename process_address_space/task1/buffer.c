#include <stdio.h>
#include <stdlib.h>

void buffFunction() {
    char* data = malloc(100 * sizeof(char));
    sprintf(data, "hello world!");
    printf("filled buffer: %s\n", data);

    free(data);
    printf("empty buffer: %s\n", data);

    sprintf(data, "hello world!");
    printf("filled buffer: %s\n", data);

    char* dataWithOffset = data + 5;
    free(dataWithOffset);
    printf("half-freed buffer: %s\n", data);
}

int main() {
    buffFunction();
    return 0;
}
