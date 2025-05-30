#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h> 

#define PORT 8080
#define QUEUE_LIM 5
#define BUFFER_SIZE 128

int main(void) {
    int main_socket, new_socket;
    int max_sockfd;

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    socklen_t addr_len = sizeof(address);
    char buffer[BUFFER_SIZE];
    fd_set read_fds;

    main_socket = socket(AF_INET, SOCK_STREAM, 0);

    int opt_val = 1;
    setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

    if(bind(main_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        printf("main_socket bind error\n");
        return 1;
    }

    listen(main_socket, QUEUE_LIM);

    FD_ZERO(&read_fds);
    FD_SET(main_socket, &read_fds);
    max_sockfd = main_socket;

    while(1) {
        fd_set temp_fds = read_fds;
        
        if(select(max_sockfd + 1, &temp_fds, NULL, NULL, NULL) < 0) {
            printf("select: no activity\n");
            return 1;
        }

        if(FD_ISSET(main_socket, &temp_fds)) {
            new_socket = accept(main_socket, (struct sockaddr*)&address, &addr_len);
            printf("newly connected socket got %d descriptor, port: %d\n", new_socket, ntohs(address.sin_port));

            FD_SET(new_socket, &read_fds);
            if(new_socket > max_sockfd) {
                max_sockfd = new_socket;
            }
        }

        for(int sockfd = 0; sockfd <= max_sockfd; sockfd++) {
            if(sockfd == main_socket || !FD_ISSET(sockfd, &temp_fds)) {
                continue;
            }

            int data_amount = read(sockfd, buffer, BUFFER_SIZE);
            if(data_amount <= 0) {
                printf("descriptor %d disconnected\n", sockfd);
                close(sockfd);
                FD_CLR(sockfd, &read_fds);
            } else {
                send(sockfd, buffer, data_amount, 0);
            }
        }
    }

    close(main_socket);

    return 0;
}
