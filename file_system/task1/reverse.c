#include <dirent.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_SIZE 100

char* getReversed(char* str) {
    size_t len = strlen(str);
    char* reverseStr = malloc((len + 1) * sizeof(char));

    for(size_t i = 0; i < len; ++i) {
        reverseStr[i] = str[len - 1 - i];
    }

    reverseStr[len] = '\0';

    return reverseStr;
}

void copyWithReverse(char* srcPath, char* dstPath) {
    FILE *src = fopen(srcPath, "rb");
    if (!src) {
        printf("Cannot open %s", srcPath);
        return;
    }

    FILE *dest = fopen(dstPath, "wb");
    if (!dest) {
        printf("Cannot open %s", dstPath);
        fclose(src);
        return;
    }

    fseek(src, 0, SEEK_END);
    long file_size = ftell(src);

    for (long i = file_size - 1; i >= 0; i--) {
        fseek(src, i, SEEK_SET);
        int byte = fgetc(src);
        fputc(byte, dest);
    }

    fclose(src);
    fclose(dest);
}

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("File path must be in arguments\n");
        return 1;
    }

    char* path = argv[1];

    char* dirName = basename(path);
    if(!dirName) {
        printf("No directory by path: %s\n",  path);
        return 1;
    }

    char* dirNameRev = getReversed(dirName);
    char pathToRev[MAX_SIZE];
    sprintf(pathToRev, "%s/%s", dirname(path), dirNameRev);

    if(mkdir(pathToRev, 0777)) {
        printf("Cannot create directory by path: %s\n", pathToRev);
        free(dirNameRev);
        return 1;
    }

    DIR* dir = opendir(path);
    if(!dir) {
        printf("Cannot open directory by path: %s\n", path);
        free(dirNameRev);
        return 1;
    }

    struct dirent* dirEntry;
    while((dirEntry = readdir(dir))) {
        char pathToSrc[MAX_SIZE];
        sprintf(pathToSrc, "%s/%s", path, dirEntry->d_name);

        struct stat st;
        if(stat(pathToSrc, &st) || S_ISDIR(st.st_mode)) {
            printf("%s isn't a file\n", pathToSrc);
            continue;
        }

        char* nameRev = getReversed(dirEntry->d_name);
        char pathToRevFile[MAX_SIZE];
        sprintf(pathToRevFile, "%s/%s", pathToRev, nameRev);

        copyWithReverse(pathToSrc, pathToRevFile);
    }

    closedir(dir);
    free(dirNameRev);

    return 0;
}
