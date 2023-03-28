#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include "helper.h"

bool in_session = false;

void *client_func(void* arg){
    char buf[BUFF_SIZE];
    thread_args* temp = arg;
    int* socketfd = temp->socketfd;
    bool* connected = temp->connected;

    while (true) {
        /* Wait to receive ACK from server */
        packet recv_pkt;
        memset(buf, 0, BUFF_SIZE);
        int res = recv(*socketfd, buf, BUFF_SIZE, 0);
        if (res == -1){
            fprintf(stdout, "ERROR: client func - recv error. \n");
            return NULL;
        }

        if (res == 0) continue; // empty recv

        /* Read ACK and deal with data */
        readPacket(&recv_pkt, buf);
        if (recv_pkt.type == 13){ // QU_ACK
            fprintf(stdout, recv_pkt.data);
        }
        else if (recv_pkt.type == 14){ // NS_NAK
            fprintf(stdout, recv_pkt.data);
        }
        else if (recv_pkt.type == 10){ // NS_ACK
            in_session = true;
            fprintf(stdout, "Create_Session Successfully! \n");
        }
        else if (recv_pkt.type == 7){ // JN_NAK
            fprintf(stdout, recv_pkt.data);
        }
        else if (recv_pkt.type == 6){ // JN_ACK
            fprintf(stdout, "Join_Session Succesfully.\n");
            in_session = true;
        }
        else if (recv_pkt.type == 11){ // MESSAGE
            fprintf(stdout, recv_pkt.data);
        }
        else if (recv_pkt.type == 18){ // TIMEOUT
            fprintf(stdout, recv_pkt.data);
            in_session = false;
            *connected = false;
            close(*socketfd);
            break;
        }
        else 
            fprintf(stdout, "ERROR: client func - receive unexpected ACK. \n");
    }
}

void login(char* tok, int* socketfd, bool* connected, pthread_t* thread){
    /* If already connected */
    if (*connected){
        fprintf(stdout, "ERROR: Already Connected.\n");
        return;
    }

    /* Tokenize Input and Read Info */
    char *id, *pwd, *ip, *port;
    tok = strtok(NULL, " ");
	id = tok;
    tok = strtok(NULL, " ");
	pwd = tok;
    tok = strtok(NULL, " ");
	ip = tok;
    tok = strtok(NULL, " ");
	port = tok;
    tok = strtok(NULL, " ");

    /* Usage Checking */
    if (id == NULL || pwd == NULL || ip == NULL || port == NULL || tok != NULL){
        fprintf(stdout, "Usage: /login id pwd ip port \n");
        return;
    }

    /* Cannot have colon in ID */
    for (char* temp = id; *temp != '\0'; temp++){
        if (*temp == ':'){
            fprintf(stdout, "ERROR: No colon in ID.\n");
            return;
        }
    }

    /* Start Connection */
    struct addrinfo hint, *res;
    memset(&hint, 0, sizeof hint);
    hint.ai_family = AF_INET; // IPv4  
    hint.ai_socktype = SOCK_STREAM; // for TCP

    if (getaddrinfo(ip, port, &hint, &res) != 0){
        fprintf(stdout, "ERROR: client getaddrinfo. \n");
        return;
    }

    /* Loop over res to find available connection */
    for(struct addrinfo *temp = res; temp != NULL; temp = temp->ai_next) {
        *socketfd = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol);
        if (*socketfd == -1) {
            fprintf(stdout, "ERROR: client socket. \n");
            continue;
        }
        if (connect(*socketfd, temp->ai_addr, temp->ai_addrlen) == -1) {
            close(*socketfd);
            fprintf(stdout, "ERROR: client connect. \n");
            continue;
        }
        *connected = true;
        break; 
    }

    /* if no available connection in res */
    if (*connected == false){
        fprintf(stdout, "ERROR: No available connection in res. \n");
        return;
    }

    /* Send login packet to server */
    packet login_info;
    login_info.type = 1;
    login_info.size = strlen(pwd);
    strncpy(login_info.source, id, MAX_NAME);
    strncpy(login_info.data, pwd, MAX_DATA);

    char buf[BUFF_SIZE];
    createPacket(&login_info, buf);
    if (send(*socketfd, buf, BUFF_SIZE, 0) == -1){
        fprintf(stdout, "ERROR: client login - send error. \n");
        close(*socketfd);
        return;
    }

    /* Wait to receive ACK from server */
    memset(buf, 0, sizeof(buf));
    if (recv(*socketfd, buf, BUFF_SIZE, 0) == -1){
        fprintf(stdout, "ERROR: client login - recv error. \n");
        close(*socketfd);
        return;
    }

    /* packet socketfd and connected into a single arg */
    thread_args *args = (thread_args *) malloc(sizeof(thread_args));
    args->connected = connected;
    args->socketfd = socketfd;

    readPacket(&login_info, buf);
    if (login_info.type == 2){ // LO_ACK
        if (pthread_create(thread, NULL, client_func, (void *)args) == 0)
        fprintf(stdout, "Login Successfully.\n");
    }
    else if (login_info.type == 3){ // LO_NAK
        // fprintf(stdout, "ERROR: client login - receiving NAK. \n");
        fprintf(stdout, login_info.data);
        *connected = false;
        close(*socketfd);
        return;
    }
    else {
        fprintf(stdout, "ERROR: client login - receiving unexpected packet type. \n");
        *connected = false;
        close(*socketfd);
        return;
    }
}

void logout (int socketfd, bool* connected, pthread_t* thread) {
    /* Must Login before Calling Logout */
    if (*connected == false){
        fprintf(stdout, "ERROR: Not yet logged in.\n");
        return;
    }

    /* Create EXIT msg to server */
    packet send_pkt;
    send_pkt.size = 0;
    send_pkt.type = 4; // EXIT

    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    createPacket(&send_pkt, buf);

    /* Send EXIT to server */
    if (send(socketfd, buf, BUFF_SIZE, 0) == -1){
        fprintf(stdout, "ERROR: client logout - send error. \n");
        return;
    }

    /* If success, cancel the thread and close socket */
    if (pthread_cancel(*thread) == -1) fprintf(stdout, "ERROR: client logout - thread cancel. \n");
    
    fprintf(stdout, "Logout Succesfully.\n");
    *connected = false;
    close(socketfd);
}

void list(int socketfd, bool connected) {
    /* Must Login before Calling List */
    if (!connected){
        fprintf(stdout, "ERROR: Not yet logged in.\n");
        return;
    }

    /* Create QUERY msg to server */
    packet send_pkt, recv_pkt;
    send_pkt.size = 0;
    send_pkt.type = 12; // QUERY

    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    createPacket(&send_pkt, buf);

    /* Send QUERY to server */
    if (send(socketfd, buf, BUFF_SIZE, 0) == -1){
        fprintf(stdout, "ERROR: client list - send error. \n");
        return;
    }
}

void createsession (char* tok, int socketfd, bool connected){
    /* If already connected */
    if (!connected){
        fprintf(stdout, "ERROR: Not yet logged in.\n");
        return;
    }

    /* Tokenize Input and Read Info */
    char *id;
    tok = strtok(NULL, " ");
	id = tok;

    /* Usage Checking */
    if (id == NULL){
        fprintf(stdout, "Usage: /createsession id\n");
        return;
    }

    /* Send NEW_SESS packet to server */
    packet send_pkt, recv_pkt;
    send_pkt.type = 9; // NEW_SESS
    send_pkt.size = strlen(id);
    strcpy(send_pkt.data, id);

    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    createPacket(&send_pkt, buf);

    /* Send NEW_SESS to server */
    if (send(socketfd, buf, BUFF_SIZE, 0) == -1){
        fprintf(stdout, "ERROR: client createsession - send error. \n");
        return;
    }
}

void leavesession (int socketfd, bool connected){
    /* If already connected */
    if (!connected){
        fprintf(stdout, "ERROR: Not yet logged in.\n");
        return;
    }

    /* If not in session */
    if (in_session == false){
        fprintf(stdout, "ERROR: Not yet in session.\n");
        return;
    }

    /* Send LEAVE_SESS packet to server */
    packet send_pkt;
    send_pkt.type = 8; // LEAVE_SESS
    send_pkt.size = 0;

    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    createPacket(&send_pkt, buf);

    /* Send LEAVE_SESS to server */
    if (send(socketfd, buf, BUFF_SIZE, 0) == -1){
        fprintf(stdout, "ERROR: client leavesession - send error. \n");
        return;
    }
    in_session = false;
    fprintf(stdout, "Leave_Session Succesfully.\n");
}

void joinsession (char* tok, int socketfd, bool connected){
    /* If already connected */
    if (!connected){
        fprintf(stdout, "ERROR: Not yet logged in.\n");
        return;
    }

    /* if already in session */
    if (in_session){
        fprintf(stdout, "ERROR: Already in session, please leave session first.\n");
        return;
    }

    /* Tokenize Input and Read Info */
    char *id;
    tok = strtok(NULL, " ");
	id = tok;

    /* Usage Checking */
    if (id == NULL){
        fprintf(stdout, "Usage: /joinsession id\n");
        return;
    }

    /* Send JOIN packet to server */
    packet send_pkt;
    send_pkt.type = 5; // JOIN
    send_pkt.size = strlen(id);
    strcpy(send_pkt.data, id);

    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    createPacket(&send_pkt, buf);

    /* Send JOIN to server */
    if (send(socketfd, buf, BUFF_SIZE, 0) == -1){
        fprintf(stdout, "ERROR: client joinsession - send error. \n");
        return;
    }
}

void reg_user(char* tok, int* socketfd, bool connected){
    /* If already connected */
    if (connected){
        fprintf(stdout, "ERROR: logout before register.\n");
        return;
    }

    /* Tokenize Input and Read Info */
    char *id, *pwd, *ip, *port;
    tok = strtok(NULL, " ");
	id = tok;
    tok = strtok(NULL, " ");
	pwd = tok;
    tok = strtok(NULL, " ");
	ip = tok;
    tok = strtok(NULL, " ");
	port = tok;
    tok = strtok(NULL, " ");

    /* Usage Checking */
    if (id == NULL || pwd == NULL || ip == NULL || port == NULL || tok != NULL){
        fprintf(stdout, "Usage: /register id pwd ip port \n");
        return;
    }

    /* Cannot have colon in ID */
    for (char* temp = id; *temp != '\0'; temp++){
        if (*temp == ':'){
            fprintf(stdout, "ERROR: No colon in ID.\n");
            return;
        }
    }

    /* Start Connection */
    struct addrinfo hint, *res;
    memset(&hint, 0, sizeof hint);
    hint.ai_family = AF_INET; // IPv4  
    hint.ai_socktype = SOCK_STREAM; // for TCP

    if (getaddrinfo(ip, port, &hint, &res) != 0){
        fprintf(stdout, "ERROR: client getaddrinfo. \n");
        return;
    }

    /* Loop over res to find available connection */
    for(struct addrinfo *temp = res; temp != NULL; temp = temp->ai_next) {
        *socketfd = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol);
        if (*socketfd == -1) {
            fprintf(stdout, "ERROR: client socket. \n");
            continue;
        }
        if (connect(*socketfd, temp->ai_addr, temp->ai_addrlen) == -1) {
            close(*socketfd);
            fprintf(stdout, "ERROR: client connect. \n");
            continue;
        }
        break; 
    }

    /* Send register packet to server */
    packet reg_info;
    reg_info.type = 15;
    reg_info.size = strlen(pwd);
    strncpy(reg_info.source, id, MAX_NAME);
    strncpy(reg_info.data, pwd, MAX_DATA);

    char buf[BUFF_SIZE];
    createPacket(&reg_info, buf);
    if (send(*socketfd, buf, BUFF_SIZE, 0) == -1){
        fprintf(stdout, "ERROR: client register - send error. \n");
        close(*socketfd);
        return;
    }

    /* Wait to receive ACK from server */
    memset(buf, 0, sizeof(buf));
    if (recv(*socketfd, buf, BUFF_SIZE, 0) == -1){
        fprintf(stdout, "ERROR: client register - recv error. \n");
        close(*socketfd);
        return;
    }

    readPacket(&reg_info, buf);
    if (reg_info.type == 16){ // REG_ACK
        fprintf(stdout, "Register Successfully!\n");
    }
    else if (reg_info.type == 17){ // REG_NAK
        // fprintf(stdout, "ERROR: client register - receiving NAK. \n");
        fprintf(stdout, reg_info.data);
        close(*socketfd);
        return;
    }
    else {
        fprintf(stdout, "ERROR: client register - receiving unexpected packet type. \n");
        close(*socketfd);
        return;
    }

    close(*socketfd);
}

int main (int argc, char *argv[]) {
    if (argc != 1){
        printf("Usage: client\n");
        return -1;
    }

    /* Variable Init */
    int socketfd;
    bool connected = false;
    pthread_t thread;
    char buffer[BUFF_SIZE];
    char cmd[BUFF_SIZE];
    char *cursor;

    /* Loop to receive user inputs */
    while (true) {
        // Clean buffers:
        memset(buffer, 0, sizeof(buffer));
        memset(cmd, 0, sizeof(buffer));

        fgets(buffer, BUFF_SIZE - 1, stdin);
        sscanf(buffer, "%[^\n]s", cmd);

        /* if buffer is empty, skip it */
        cursor = buffer;
        while (*cursor == ' ') cursor++;
        if (*cursor == '\n') continue;

        /* Tokenize input */
        cursor = strtok(cmd, " ");
        
        if (strcmp(cursor, "/login") == 0){
            login(cursor, &socketfd, &connected, &thread);
        }
        else if (strcmp(cursor, "/logout") == 0){
            logout(socketfd, &connected, &thread);
        }
        else if (strcmp(cursor, "/createsession") == 0){
            createsession(cursor, socketfd, connected);
        }
        else if (strcmp(cursor, "/leavesession") == 0){
            leavesession(socketfd, connected);
        }
        else if (strcmp(cursor, "/joinsession") == 0){
            joinsession(cursor, socketfd, connected);
        }
        else if (strcmp(cursor, "/list") == 0){
            list(socketfd, connected);
        }
        else if (strcmp(cursor, "/quit") == 0){
            /*Automatically Logout before quiting program */
            if (connected) logout(socketfd, &connected, &thread);
            break;
        }
        else if (strcmp(cursor, "/register") == 0){
            reg_user(cursor, &socketfd, connected);
        }
        /* Send Message to Server */
        else {
            /* cannot send before login */
            if (!connected){
                fprintf(stdout, "ERROR: Not yet logged in.\n");
                continue;
            }

            if (!in_session){
                fprintf(stdout, "ERROR: Join a session before sending messages.\n");
                continue;
            }
                
            /* Send MSG packet to server */
            packet send_pkt;
            send_pkt.type = 11; // MESSAGE
            send_pkt.size = strlen(buffer);
            strcpy(send_pkt.data, buffer);

            char buf[BUFF_SIZE];
            memset(buf, 0, BUFF_SIZE);
            createPacket(&send_pkt, buf);

            /* Send MSG to server */
            if (send(socketfd, buf, BUFF_SIZE, 0) == -1){
                fprintf(stdout, "ERROR: client message - send error. \n");
                return -1;
            }
        }
    }

    fprintf(stdout, "Exit Successfully! \n");

    return 0;
}