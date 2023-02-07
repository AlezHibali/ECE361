#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <time.h>
#include "packet.h"

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
    if (file = fopen(filename, "rb")){
        fclose(file);
    }
    else {
        printf("File does not exist.\n");
        return -1;
    }

    /* Start Timer */
    clock_t send, recv;
    double cpu_time_used;
    send = clock();
    
    /* if exist, send msg to server */
    long send_size = sendto(socketfd, "ftp", strlen("ftp"), 0, (struct sockaddr*)&server_info, server_len);
    if (send_size < 0){
        printf("Message Sending Error.\n");
        return -1;
    }

    /* Receive message */
    long recv_size = recvfrom(socketfd, server_msg, sizeof(server_msg), 0, (struct sockaddr*)&server_info, &server_len);
    if (recv_size < 0){
        printf("Message Receive Error.\n");
        return -1;
    }

    /* Stop Timer and Calculate RTT */
    recv = clock();
    cpu_time_used = ((double) (recv - send)) / CLOCKS_PER_SEC;
    printf("RTT = %f seconds.\n", cpu_time_used);

    /* Final Output */
    if (strcmp(server_msg, "yes"))
        printf("A file transfer can start.\n");
    else
        return -1;

    /* Start Lab2: Transfering using packets */

    /* Open file and Calculate its size for packet division */
    file = fopen(filename, "r");
    fseek(file, 0, SEEK_END);
    long total_size = ftell(file);
    int total_frag = total_size / 1000 + 1;
    fseek(file, 0, SEEK_SET); // back to beginning of file

    /* Packet Division */
    char** packet_list = malloc(sizeof(char*) * total_frag);
    for (int frag_no = 1; frag_no <= total_frag; frag_no++){
        // create pkt and clear buffer
        packet pkt;
        memset(pkt.filedata, 0, 1000);

        // update info
        pkt.total_frag = total_frag;
        pkt.frag_no = frag_no;
        pkt.filename = filename;

        // upload data
        int read_byte = fread((void*)pkt.filedata, 1, 1000, file);
        if (read_byte <= 0){
            printf("Read Error!\n");
            return -1;
        }

        if (frag_no != total_frag)
            pkt.size = 1000;
        else
        {
            pkt.size = total_size % 1000;
            if (pkt.size == 0) pkt.size = 1000;
        }
        
        /* Create packets and insert into list */
        packet_list[frag_no-1] = malloc(strlen(filename)+BUFF_SIZE);
        createPacket(&pkt, packet_list[frag_no-1]);
    }

    packet ack_pkt;
    ack_pkt.filename = (char *)malloc(100 * sizeof(char));
    int time_sent = 0;

    for (int frag_no = 1; frag_no <= total_frag; frag_no++){
        /* Send packets */
        long sent = sendto(socketfd, packet_list[frag_no-1], BUFF_SIZE, 0, (struct sockaddr*)&server_info, server_len);
        if (sent < 0){
            printf("Packet Sending Error.\n");
            return -1;
        }

        /* ACK receiving */
        memset(server_msg, 0, sizeof(server_msg));
        if (recvfrom(socketfd, server_msg, sizeof(server_msg), 0, (struct sockaddr*)&server_info, &server_len)<0){
            if (time_sent > 50){
                printf("Time Out / Too many Resends\n");
                return -1;
            }
            printf("Error Receiving ACK, Try Resending\n");
            frag_no--; time_sent++;
            continue;
        }

        // Analyze packet and Continue Sending
        readPacket(&ack_pkt, server_msg);
        if(strcmp(ack_pkt.filename, filename) == 0
            && strcmp(ack_pkt.filedata, "ACK") == 0
            && ack_pkt.frag_no == frag_no) {
            // printf("ACK received ok for %s frag %d\n", filename, frag_no);
        }
        else if (time_sent > 50){
            printf("Time Out / Too many Resends\n");
            return -1;
        }
        else{
            printf("ACK receive error, trying resend\n");
            // printf("ACK received not ok for %s frag %d\n", filename, frag_no);
            // printf("ACK received is %s frag %d\n", ack_pkt.filename, ack_pkt.frag_no);
            frag_no--; time_sent++;
            continue;
        }

        // if successfully sent, reset time_sent
        time_sent = 0;
    }

    for(int packet_num = 1; packet_num <= total_frag; ++packet_num) {
        free(packet_list[packet_num - 1]);
    }
    free(packet_list);

    free(ack_pkt.filename);

    fclose(file);

    return 0;
}
