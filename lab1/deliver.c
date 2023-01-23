#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdbool.h>


int main (int argc, char *argv[]) {
    if (argc != 3){
        printf("deliver <server address> <server port number>\n");
        return -1;
    }

    int port = atoi(argv[2]);
    int socketfd;
    struct sockaddr_in server_info;
    char server_msg[4096], client_msg[4096], filename[4096];
    socklen_t server_len = sizeof(server_info);
    
    // Clean buffers:
    memset(server_msg, '\0', sizeof(server_msg));
    memset(client_msg, '\0', sizeof(client_msg));

    /* socket initialization */
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0){
        printf("Error with socket file descriptor creation.\n");
        return -1;
    }

    /* IP and Port number */
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(port);
    if (inet_aton(argv[1] , &server_info.sin_addr) == 0) {
        printf("Error in ip address.\n");
        return -1;
    }

    /* Wait for User/Client Input */
    fprintf(stdout, "Input as following format: ftp filename\n");
    fgets(client_msg, sizeof(client_msg), stdin);

    /* skip all spaces and check for ftp */
    int index = 0;
    while (client_msg[index] == ' ') index++;
    
    if (client_msg[index] != 'f' || client_msg[index+1] != 't' || client_msg[index+2] != 'p'){
        printf("No ftp for input\n");
        return -1;
    }

    /* Get filename*/
    index += 3;
    while (client_msg[index] == ' ') index++;
    char *token = strtok(client_msg + index, "\r\t\n ");
    strncpy(filename, token, sizeof(filename));

    /* Check for existence of file */
    FILE *file;
    if (file = fopen(filename, "r")){
        fclose(file);
    }
    else {
        printf("File does not exist.\n");
        return -1;
    }
    /* if exist, send msg to server */
    long send_size = sendto(socketfd, "ftp", strlen("ftp"), 0, (struct sockaddr*)&server_info, server_len);
    if (send_size < 0){
        printf("Message Sending Error.\n");
        return -1;
    }

    /* Receive message */
    long recv_size = recvfrom(socketfd, server_msg, sizeof(client_msg), 0, (struct sockaddr*)&server_info, &server_len);
    if (recv_size < 0){
        printf("Message Receive Error.\n");
        return -1;
    }

    /* Final Output */
    if (strcmp(server_msg, "yes"))
        printf("A file transfer can start.\n");
    else
        return -1;

    return 0;
}
