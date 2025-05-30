#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"
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
    
    while(1) {
        char send_buffer[128];
        scanf("%s", send_buffer);
        sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        memset(send_buffer, '\0', 128);

        char recv_buffer[128];
        socklen_t server_addr_len = sizeof(server_addr); 
        ssize_t bytes_received = recvfrom(sockfd, recv_buffer, SIZE, 0, (struct sockaddr*)&server_addr, &server_addr_len);

        printf("answer: %s\n", recv_buffer);
        memset(recv_buffer, '\0', 128);
    }

    close(sockfd);
    
    return 0;
}
