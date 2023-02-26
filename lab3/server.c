#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include "packet.h"

int main (int argc, char *argv[]) {
    if (argc != 2){
        printf("server <UDP listen port>\n");
        return -1;
    }

    // seed set
    srand(time(NULL));

    int port = atoi(argv[1]);
    int socketfd;
    struct sockaddr_in server_info, client_info;
    char server_msg[4096], client_msg[4096], filename[4096];
    socklen_t client_len = sizeof(client_info);

    // Clean buffers:
    memset(server_msg, 0, sizeof(server_msg));
    memset(client_msg, 0, sizeof(client_msg));
    
    /* socket initialization */
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0){
        printf("Error with socket file descriptor creation.\n");
        return -1;
    }

    /* IP and Port number */
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(port);
    server_info.sin_addr.s_addr = htonl(INADDR_ANY);

    /* socket binding */
    int bind_res = bind(socketfd, (struct sockaddr*)&server_info, sizeof(server_info));
    if (bind_res < 0){
        printf("Error with bind. Try with different port num.\n");
        return -1;
    }

    /* Listen and Receive message */
    long recv_size = recvfrom(socketfd, client_msg, sizeof(client_msg), 0, (struct sockaddr*)&client_info, &client_len);
    if (recv_size < 0){
        printf("Message Receive Error.\n");
        return -1;
    }

    /* Respond to Client */
    bool response = strcmp(client_msg, "ftp");
    if (response) strcpy(server_msg, "yes");
    else strcpy(server_msg, "no");

    long send_size = sendto(socketfd, server_msg, strlen(server_msg), 0, (struct sockaddr*)&client_info, client_len);
    if (send_size < 0){
        printf("Message Sending Error.\n");
        return -1;
    }

    /* Start Lab2: Transfering using packets */
    FILE *new_file;
    packet recv_pkt;
    bool first_time = true;

    /* Receive packets */
    while (true) {
        long recv_s = recvfrom(socketfd, client_msg, BUFF_SIZE, 0, (struct sockaddr*)&client_info, &client_len);
        if (recv_s < 0){
            printf("Message Receive Error.\n");
            return -1;
        }
        
        readPacket(&recv_pkt, client_msg);

        // build new files, build with suffix if existed
        if (first_time){
            first_time = false;
            char temp;
            strcpy(filename, recv_pkt.filename);

            char* suffix_ptr = strrchr(recv_pkt.filename, '.'); // find things after the last dot
            char suffix[30] = {0};
            strncpy(suffix, suffix_ptr, strlen(suffix_ptr)+1);
            temp = *suffix_ptr;
            *suffix_ptr = '\0'; // set a stop at filename

            int i = 1;
            while (access(filename, F_OK) == 0) {
                snprintf(filename, 130, "%s_%d%s", recv_pkt.filename, i, suffix);
                i++;
            } 

            new_file = fopen(filename, "wb");
            *suffix_ptr = temp;
        }

        /* Lab3: Add a possibility that 1% of the packets are dropped */
        // rand from 0 to 99, if 0 drop packet, otherwise continue proceeding
        if (rand()%100 == 0){
            printf("Packet #%d is dropped.\n", recv_pkt.frag_no);
            continue;
        }

        // write into new file
        int byte_write = fwrite(recv_pkt.filedata, 1, recv_pkt.size, new_file);

        if (byte_write != recv_pkt.size){
            printf("New File Write Error.\n");
            return -1;
        }

        // ACK creation
        memset(server_msg, 0, sizeof(server_msg));
        strcpy(recv_pkt.filedata, "ACK");
        createPacket(&recv_pkt, server_msg);

        // ACK sending
        int sent = sendto(socketfd, server_msg, strlen(server_msg), 0, (struct sockaddr*)&client_info, client_len);
        if (sent < 0){
            printf("Message Sending Error.\n");
            return -1;
        }

        // end condition
        if (recv_pkt.frag_no == recv_pkt.total_frag) {
            printf("Completion!\n");
            break;
        }
    }

    close(socketfd);
    fclose(new_file);
    free(recv_pkt.filename);

    return 0;
}
