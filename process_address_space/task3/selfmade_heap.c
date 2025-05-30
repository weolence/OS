#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

#define HEAP_SIZE 1024

typedef struct block {
    size_t size;
    char isFree;
    struct block* next;
} block;

void* heapBase = NULL;
block* head = NULL;

void heapInit() {
     heapBase = mmap(NULL, HEAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
     if(heapBase == MAP_FAILED) {
        printf("mmap error\n");
        _exit(1);
     }

     head = heapBase;
     head->size = HEAP_SIZE - sizeof(block);
     head->isFree = 1;
     head->next = NULL;
}

void* myMalloc(size_t size) {
    if(!size) {
        return NULL;
    }
    
    if(!heapBase) {
        heapInit();
    }
 
    size = (size + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1);

    block* temp = head;
    block* bestPlace = NULL;

    while(temp) {
        // ищем незанятый блок с достаточным кол-вом памяти
        if(temp->isFree && temp->size >= size) {
            // если находится блок меньшего размера чем лучший подобранный на
            // данный момент из подходящих то берём его
            if(!bestPlace || temp->size < bestPlace->size) {
                bestPlace = temp;
            }
        }
        temp = temp->next;
    }

    if(!bestPlace) {
        printf("not enough memory\n");
        return NULL;
    }
    
    // если оказывается что в выбранном лучшем блоке места куда больше чем нам
    // нужно(размер, который хотим в байтах + размер характ. структуры) то
    // отделяем лишнее для экономии 
    if(bestPlace->size > size + sizeof(block)) {
        block* newBlock = (block*)((char*)bestPlace + sizeof(block) + size);
        newBlock->size = bestPlace->size - (size + sizeof(block));
        newBlock->isFree = 1;
        newBlock->next = bestPlace->next;

        bestPlace->size = size;
        bestPlace->next = newBlock;
    }

    bestPlace->isFree = 0;

    return (void*)((char*)bestPlace + sizeof(block));
}

void myFree(void* ptr) {
    if(!ptr) {
        printf("pointer already freed\n");
        return;
    }

    block* header = (block*)((char*)ptr - sizeof(block));
    header->isFree = 1;

    block* temp = head;
    while(temp && temp->next) {
        if(temp->isFree && temp->next->isFree) {
            temp->size += temp->next->size + sizeof(block);
            temp->next = temp->next->next;
        } else { 
            temp = temp->next;
        }
    }
}

int main(void) {
    int* ptrs[3];
    int* arr;
    for(int i = 0; i < 3; ++i) {
        arr = myMalloc(400);
        printf("%p\n", arr);
        ptrs[i] = arr;
    }

    myFree(ptrs[1]);

    arr = myMalloc(500);
    printf("%p\n", arr);

    return 0;
}
