#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#define PORT 8080
#define SIZE 128
#define QUEUE_LIMIT 5

int main(void) {
    int serverfd, clientfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if(serverfd < 0) {
        printf("server descriptor error\n");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(serverfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("server binding error\n");
        close(serverfd);
        return 1;
    }

    listen(serverfd, QUEUE_LIMIT);

    while(1) {
        clientfd = accept(serverfd, (struct sockaddr*)&client_addr, &client_len);
        if(clientfd < 0) {
            printf("client descriptor error\n");
            close(serverfd);
            return 1;
        }

        pid_t pid = fork();

        if(pid == 0) {
            close(serverfd);

            char buffer[SIZE];
            ssize_t bytes_read;

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            while((bytes_read = read(clientfd, buffer, sizeof(buffer)))) {
                write(clientfd, buffer, bytes_read);
                printf("replied to: %s:%d\n",  client_ip, htons(client_addr.sin_port));
                memset(buffer, '\0', sizeof(buffer));
            }

            close(clientfd);

            printf("process %d closed\n", getpid());

            return 0;
        } else {
            printf("process %d created\n", pid);
        }
    }

    close(serverfd);

    return 0;
}
