#ifndef PACKET_H
#define PACKET_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define BUFF_SIZE 1100

typedef struct Packet {
    unsigned int total_frag;  
    unsigned int frag_no; 
    unsigned int size; 
    char* filename; 
    char filedata[1000];
} packet;

void createPacket (const packet *packet, char* buff){
    // Clean buffers:
    memset(buff, 0, sizeof(buff));

    // load info respectively
    sprintf(buff, "%u:%u:%u:%s:", packet->total_frag, packet->frag_no, packet->size, packet->filename);

    // id track locationload filedata
    int id = strlen(buff);

    //for(int i = 0; i < packet->size; i++){
    //    *(buff+id+i) = packet->filedata[i];
    //}
    memcpy(buff+id, packet->filedata, packet->size);
}

void readPacket (packet *packet, const char* buff){
    int total_frag, frag_no, size;
    char filename[100];

    sscanf(buff, "%d:%d:%d:%[^:]", &total_frag, &frag_no, &size, filename);

    // update info in packet
    packet->total_frag = (unsigned int)total_frag;
    packet->frag_no = (unsigned int)frag_no;
    packet->size = (unsigned int)size;
    packet->filename = (char*)malloc(strlen(filename) + 1);
    strcpy(packet->filename, filename);

    // update filedata from buffer
    const char *start = strchr(buff, ':') + 1;
    start = strchr(start, ':') + 1;
    start = strchr(start, ':') + 1;
    start = strchr(start, ':') + 1;

    //for(int i = 0; i < packet->size; i++){
    //    packet->filedata[i] = *(start+i);
    //}
    memcpy(packet->filedata, start, packet->size);
}

#endif