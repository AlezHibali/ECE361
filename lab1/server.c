#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdbool.h>

int main (int argc, char *argv[]) {
    if (argc != 2){
        printf("server <UDP listen port>");
        return -1;
    }

    int port = atoi(argv[1]);
    int socketfd;
    struct sockaddr_in server_info, client_info;
    char server_msg[4096], client_msg[4096];
    socklen_t client_len = sizeof(client_info);

    // Clean buffers:
    memset(server_msg, '\0', sizeof(server_msg));
    memset(client_msg, '\0', sizeof(client_msg));
    
    /* socket initialization */
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0){
        printf("Error with socket file descriptor creation.");
        return -1;
    }

    /* IP and Port number */
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(port);
    server_info.sin_addr.s_addr = htonl("128.100.13.139");

    /* socket binding */
    int bind_res = bind(socketfd, (struct sockaddr*)&server_info, sizeof(server_info));
    if (bind_res < 0){
        printf("Error with bind. Try with different port num.");
        return -1;
    }

    /* Listen and Receive message */
    long recv_size = recvfrom(socketfd, client_msg, sizeof(client_msg), 0, (struct sockaddr*)&client_info, &client_len);
    if (recv_size < 0){
        printf("Message Receive Error.");
        return -1;
    }

    /* Respond to Client */
    bool response = (client_msg == "ftp");
    if (response) strcpy(server_msg, "yes");
    else strcpy(server_msg, "no");

    long send_size = sendto(socketfd, server_msg, strlen(server_msg), 0, (struct sockaddr*)&client_info, &client_len);
    if (send_size < 0){
        printf("Message Sending Error.");
        return -1;
    }

    close(socketfd);

    return 0;
}
