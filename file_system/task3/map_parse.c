#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#define PAGE_SIZE 4096
#define PATH_SIZE 256

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("Number of process needed as argument\n");
        return 1;
    }

    pid_t pid = strtoull(argv[1], NULL, 10);
    char mapsPath[PATH_SIZE];
    char pagemapPath[PATH_SIZE];

    snprintf(mapsPath, sizeof(mapsPath), "/proc/%d/maps", pid);
    snprintf(pagemapPath, sizeof(pagemapPath), "/proc/%d/pagemap", pid);

    FILE* mapsFile = fopen(mapsPath, "r");
    if(!mapsFile) {
        printf("cannot open maps file\n");
        return 1;
    }
    
    int pagemapFD = open(pagemapPath, O_RDONLY);
    if(pagemapFD < 0) {
        printf("cannot open pagemap file\n");
        fclose(mapsFile);
        return 1;
    }

    char mapsLine[256];
    while(fgets(mapsLine, sizeof(mapsLine), mapsFile)) {
        uint64_t begin = 0, end = 0;
        char path[PATH_SIZE];
        int n = sscanf(mapsLine, "%lx-%lx", &begin, &end);
        if(n < 2) {
            printf("unable to count address\n");
            continue;
        }

        for(uint64_t address = begin; address < end; address += PAGE_SIZE) {
            uint64_t entry = 0;
            off_t pos = address / PAGE_SIZE * sizeof(entry);
            
            ssize_t k = pread(pagemapFD, &entry, sizeof(entry), pos);
            if(k != sizeof(entry)) {
                printf("cannot read page\n");
                continue;
            }

            uint64_t pfn = entry & 0x7FFFFFFFFFFFFF;
            uint64_t present = (entry >> 63) & 1;
            uint64_t swapped = (entry >> 62) & 1;
            uint64_t soft_dirty = (entry >> 55) & 1;
            uint64_t exclusive = (entry >> 56) & 1;
            uint64_t file_shared = (entry >> 61) & 1;

            printf("Virtual Address: 0x%llx\n", (unsigned long long)address);
            printf("  PFN: %llu\n", (unsigned long long)pfn);
            printf("  Present: %llu\n", (unsigned long long)present);
            printf("  Swapped: %llu\n", (unsigned long long)swapped);
            printf("  Soft Dirty: %llu\n", (unsigned long long)soft_dirty);
            printf("  Exclusive: %llu\n", (unsigned long long)exclusive);
            printf("  File/Shared: %llu\n\n", (unsigned long long)file_shared);
        }
    }

    fclose(mapsFile);
    close(pagemapFD);

    return 0;
}
