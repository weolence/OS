#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>

#define MAX_SIZE 1024

void create_directory(const char *path) {
    if(mkdir(path, 0777)) {
        printf("Cannot create dir by path %s\n", path);
    }
}

void list_directory(const char *path) {
    DIR *dir = opendir(path);
    if(!dir) {
        printf("Cannot open dir by path %s\n", path);
        return;
    }

    struct dirent *entry;
    while((entry = readdir(dir))) {
        printf("%s\n", entry->d_name);
    }

    closedir(dir);
}

void remove_directory(const char *path) {
    if(rmdir(path)) {
        printf("No dir by path %s\n", path);
    }
}

void create_file(const char *path) {
    FILE* file = fopen(path, "a");
    if(!file) {
        printf("Cannot create file by path %s\n", path);
        return;
    }
    fclose(file);
}

void read_file(const char *path) {
    FILE *file = fopen(path, "r");
    if(!file) {
        printf("Cannot open file by path %s\n", path);
        return;
    }

    char ch;
    while((ch = fgetc(file)) != EOF) {
        putchar(ch);
    }

    fclose(file);
}

void remove_file(const char *path) {
    if(remove(path)) {
        printf("Cannot remove file by path %s\n", path);
    }
}

void create_symlink(const char *target, const char *linkname) {
    if(symlink(target, linkname)) {
        printf("Cannot create symbol link for %s\n", target);
    }
}

void read_symlink(const char *linkname) {
    char target[MAX_SIZE];
    int len = (int)readlink(linkname, target, sizeof(target) - 1);
    if(len == -1) {
        printf("Cannot read symbol link by name %s\n", linkname);
    } else {
        target[len] = '\0';
        printf("Target of %s is %s\n", linkname, target);
    }
}

void remove_symlink(const char *linkname) {
    if(unlink(linkname)) {
        printf("Cannot remove symbol link %s\n", linkname);
    }
}

void create_hardlink(const char *target, const char *linkname) {
    if(link(target, linkname)) {
        printf("Cannot create hard link to %s\n", target);
    }
}

void remove_hardlink(const char *linkname) {
    if(unlink(linkname)) {
        printf("Cannot remove hard link %s\n", linkname);
    }
}

void file_info(const char *path) {
    struct stat st;
    if(stat(path, &st)) {
        printf("Cannot find file by path %s\n", path);
        return;
    }

    printf("File permissions: ");
    printf((S_ISDIR(st.st_mode)) ? "d" : "-");
    printf((st.st_mode & S_IRUSR) ? "r" : "-");
    printf((st.st_mode & S_IWUSR) ? "w" : "-");
    printf((st.st_mode & S_IXUSR) ? "x" : "-");
    printf((st.st_mode & S_IRGRP) ? "r" : "-");
    printf((st.st_mode & S_IWGRP) ? "w" : "-");
    printf((st.st_mode & S_IXGRP) ? "x" : "-");
    printf((st.st_mode & S_IROTH) ? "r" : "-");
    printf((st.st_mode & S_IWOTH) ? "w" : "-");
    printf((st.st_mode & S_IXOTH) ? "x" : "-");
    printf("\n");

    printf("Hard links count: %ld\n", st.st_nlink);
}

void change_permissions(const char *path, mode_t mode) {
    if(chmod(path, mode)) {
        printf("Cannot change permissions for file %s", path);
    }
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Not enough arguments\n");
        return 1;
    }

    const char *command = basename(argv[0]);
    const char *path = argv[1];

    if(strcmp(command, "create_directory") == 0) {
        create_directory(path);
    } else if(strcmp(command, "list_directory") == 0) {
        list_directory(path);
    } else if(strcmp(command, "remove_directory") == 0) {
        remove_directory(path);
    } else if(strcmp(command, "create_file") == 0) {
        create_file(path);
    } else if(strcmp(command, "read_file") == 0) {
        read_file(path);
    } else if(strcmp(command, "remove_file") == 0) {
        remove_file(path);
    } else if(strcmp(command, "create_symlink") == 0) {
        create_symlink(path, argv[2]);
    } else if(strcmp(command, "read_symlink") == 0) {
        read_symlink(path);
    } else if(strcmp(command, "remove_symlink") == 0) {
        remove_symlink(path);
    } else if(strcmp(command, "create_hardlink") == 0) {
        create_hardlink(path, argv[2]);
    } else if(strcmp(command, "remove_hardlink") == 0) {
        remove_hardlink(path);
    } else if(strcmp(command, "file_info") == 0) {
        file_info(path);
    } else if(strcmp(command, "change_permissions") == 0) {
        mode_t new_mode = strtol(argv[2], NULL, 8);
        change_permissions(path, new_mode);
    } else {
        printf("Unknown command: %s\n", command);
    }

    return 0;
}
