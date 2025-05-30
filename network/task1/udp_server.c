#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define SIZE 128

int main(void) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0) {
        printf("descriptor error\n");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_addr.s_addr = INADDR_ANY; // listening to any address
    server_addr.sin_port = htons(PORT); // host to network short
    server_addr.sin_family = AF_INET; // type of IP address(IPv4)

    if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("bind error\n");
        close(sockfd);
        return 1;
    }

    while(1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        char buffer[SIZE];
        
        ssize_t bytes_received = recvfrom(sockfd, buffer, SIZE, 0, (struct sockaddr*)&client_addr, &client_addr_len);

        sendto(sockfd, buffer, bytes_received, 0, (struct sockaddr*)&client_addr, client_addr_len);
    }

    close(sockfd);

    return 0;
}
