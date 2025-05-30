#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define SIZE 128

int main(void) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        printf("descriptor error\n");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);
    server_addr.sin_family = AF_INET;

    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("connection error\n");
        close(sockfd);
        return 1;
    }

    while(1) {
        char send_buffer[SIZE];
        scanf("%s", send_buffer);
        write(sockfd, send_buffer, strlen(send_buffer));
        memset(send_buffer, '\0', sizeof(send_buffer));

        char recv_buffer[SIZE];
        socklen_t server_addr_len = sizeof(server_addr); 
        ssize_t bytes_received = read(sockfd, recv_buffer, sizeof(recv_buffer));

        printf("answer: %s\n", recv_buffer);
        memset(recv_buffer, '\0', SIZE);
    }

    close(sockfd);

    return 0;
}
