#ifndef PACKET_H
#define PACKET_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define BUFF_SIZE 1400
#define MAX_NAME 128
#define MAX_DATA 1024
#define MAX_USER 10

// 1 LOGIN  <password> Login with the server 
// 2 LO_ACK  Acknowledge successful login 
// 3 LO_NAK <reason for failure> Negative acknowledgement of login 
// 4 EXIT  Exit from the server 
// 5 JOIN <session ID> Join a conference session 
// 6 JN_ACK <session ID> Acknowledge successful conference session join 
// 7 JN_NAK <session ID, reason for failure> Negative acknowledgement of joining the session 
// 8 LEAVE_SESS   Leave a conference session 
// 9 NEW_SESS <session ID> Create new conference session 
// 10 NS_ACK  Acknowledge new conference session 
// 11 MESSAGE <message data> Send a message to the session or display the message if it is received 
// 12 QUERY  Get a list of online users and available sessions 
// 13 QU_ACK <users and sessions> Reply followed by a list of users online 

// 14 NS_NAK  NAK new conference session

typedef struct message {
    unsigned int type;  
    unsigned int size; 
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
} packet;

typedef struct User {
    bool connected;
    bool in_session;
    int socketfd;
    unsigned char id[MAX_NAME];
    unsigned char pwd[MAX_DATA];
    unsigned char session_id[MAX_DATA];
} user;

typedef struct Conference {
    unsigned char id[MAX_DATA];
    unsigned char user_id_list[MAX_USER][MAX_NAME];
    int socketfd_list[MAX_USER];
    int user_count;
} session;

void createPacket (const packet *packet, char* buff){
    // Clean buffers:
    memset(buff, 0, sizeof(buff));

    // load info respectively
    sprintf(buff, "%u:%u:%s:", packet->type, packet->size, packet->source);

    // id track location + load data
    int id = strlen(buff);

    memcpy(buff+id, packet->data, packet->size);
}

void readPacket (packet *packet, const char* buff){
    int type, size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];

    // Clean buffers:
    memset(&packet->source, 0, MAX_NAME);
    memset(&packet->data, 0, MAX_DATA);

    sscanf(buff, "%d:%d:%[^:]", &type, &size, source);

    // update info in packet
    packet->type = (unsigned int)type;
    packet->size = (unsigned int)size;

    // load source name
    strncpy(packet->source, source, MAX_NAME);

    // update data from buffer
    const char *start = strchr(buff, ':') + 1;
    start = strchr(start, ':') + 1;
    start = strchr(start, ':') + 1;

    memcpy(packet->data, start, packet->size);
}

#endif