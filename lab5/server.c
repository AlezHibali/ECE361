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
#include <netdb.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/time.h>
#include "helper.h"
#include "sys/select.h"

#define QUEUE_SIZE 10
#define MAX_SESSION 10

user user_list[MAX_USER];
session session_list[MAX_SESSION];

pthread_mutex_t session_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t user_lock = PTHREAD_MUTEX_INITIALIZER;

void add_user(char id[], char pwd[], int* sockfd){
    int socketfd = *sockfd;
    pthread_mutex_lock(&user_lock);
    /* Loop over user list and find available slot */
    for (int i = 0; i < MAX_USER; i++){
        if (user_list[i].connected == false){
            /* create user info */
            user_list[i].connected = true;
            user_list[i].in_session = false;
            user_list[i].socketfd = socketfd;
            strncpy(user_list[i].id, (char *)id, MAX_NAME);
            strncpy(user_list[i].pwd, (char *)pwd, MAX_DATA);

            fprintf(stdout, "User [%s] Login!\n", id);
            pthread_mutex_unlock(&user_lock);
            return;
        }
    }
    fprintf(stdout, "ERROR: User is Full - cannot add more user! \n");
    pthread_mutex_unlock(&user_lock);
}

void delete_user(char id[]) {
    /* NULL name -> no delete */
    if (id[0] == '\0')
        return;
    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < MAX_USER; i++){
        if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
            user_list[i].connected = false;
            user_list[i].in_session = false;
            fprintf(stdout, "User [%s] Logout! \n", id);
            pthread_mutex_unlock(&user_lock);
            return;
        }
    }
    fprintf(stdout, "ERROR: No ID found for to-be-deleted user [%s]! \n", id);
    pthread_mutex_unlock(&user_lock);
}

bool register_user(const char *filename, const char *user, const char *pwd){
    ID_PWD users[MAX_USER];
    int num_users = 0;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stdout, "ERROR: cannot open file %s\n", filename);
        return false;
    }

    /* read and check user list */
    unsigned char buffer[BUFF_SIZE];
    unsigned char username[MAX_NAME];
    unsigned char password[MAX_DATA];
    memset(buffer, 0, BUFF_SIZE);

    while (fgets(buffer, BUFF_SIZE, fp) != NULL && num_users < MAX_USER) {
        /* clear buffer */
        memset(username, 0, MAX_NAME);
        memset(password, 0, MAX_DATA);

        if (sscanf(buffer, "%s %s", username, password) != 2) {
            fprintf(stdout, "ERROR: register sscanf return error\n");
            return false;
        } else {
            strncpy(users[num_users].id, username, sizeof(username));
            strncpy(users[num_users].pwd, password, sizeof(password));
            num_users++;
        }
    }

    fclose(fp);

    /* check if username match */
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].id, user) == 0){
            return false; // username exists
        } 
    }

    /* Write username and pwd to the file */
    fp = fopen(filename, "a"); // append
    if (fp == NULL) {
        fprintf(stdout, "ERROR: cannot write to file %s\n", filename);
        return false;
    }

    fprintf(fp, "%s %s\n", user, pwd);
    fclose(fp);

    return true;
}

int authentication(const char *filename, const char *user, const char *pwd){
    ID_PWD users[MAX_USER];
    int num_users = 0;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stdout, "ERROR: cannot open file %s\n", filename);
        return 2;
    }

    /* read and store user list */
    unsigned char buffer[BUFF_SIZE];
    unsigned char username[MAX_NAME];
    unsigned char password[MAX_DATA];
    memset(buffer, 0, BUFF_SIZE);

    while (fgets(buffer, BUFF_SIZE, fp) != NULL && num_users < MAX_USER) {
        /* clear buffer */
        memset(username, 0, MAX_NAME);
        memset(password, 0, MAX_DATA);

        if (sscanf(buffer, "%s %s", username, password) != 2) {
            fprintf(stdout, "ERROR: authentication sscanf return error\n");
        } else {
            strncpy(users[num_users].id, username, sizeof(username));
            strncpy(users[num_users].pwd, password, sizeof(password));
            num_users++;
        }
    }

    fclose(fp);

    /* check if username and password match */
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].id, user) == 0){
            /* found user, check pwd */
            if (strcmp(users[i].pwd, pwd) == 0)
                return 0;
            else
                return 1; // wrong pwd
        } 
    }

    return 2; // not registered
}

void leave_session(char id[]){
    char session_id[MAX_DATA];
    memset(session_id, 0, MAX_DATA);

    /* first check if user is in session */
    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < MAX_USER; i++){
        if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
            /* set info in user list and get session id */
            user_list[i].in_session = false;
            strcpy(session_id, user_list[i].session_id);
            memset(user_list[i].session_id, 0, MAX_DATA);
            break;
        }
    }
    pthread_mutex_unlock(&user_lock);

    /* Remove user from session */
    pthread_mutex_lock(&session_lock);

    /* Loop to find existing session slot */
    for (int i = 0; i < MAX_SESSION; i++) {
        if (session_list[i].user_count != 0){
            if (strcmp(session_list[i].id, session_id) == 0){
                /* decrement count and delete user from list */
                session_list[i].user_count -= 1;

                for (int j = 0; j < MAX_USER; j++){
                    if (strcmp(session_list[i].user_id_list[j], id) == 0){
                        memset(session_list[i].user_id_list[j], 0, MAX_NAME);
                        session_list[i].socketfd_list[j] = -1;
                        fprintf(stdout, "User [%s] leaves Session [%s].\n", id, session_id);
                        break;
                    }
                }

                /* delete session if no user inside */
                if (session_list[i].user_count == 0)
                    fprintf(stdout, "Session [%s] deleted!\n", session_id);

                break;
            }
        }
    }
    pthread_mutex_unlock(&session_lock);
}

int create_session(char data[], char id[], int* sockfd){
    int index;
    int socketfd = *sockfd;

    /* First check if the user is in session */
    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < MAX_USER; i++){
        if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
            /* if user has already been in session */
            if (user_list[i].in_session){
                // fprintf(stdout, "ERROR: User already in session - cannot create session! \n");
                pthread_mutex_unlock(&user_lock);
                return 1;
            }
            /* if not in session, go to create */
            else {
                index = i;
                break;
            }
        }
    }
    pthread_mutex_unlock(&user_lock);

    /* Find if there is existed session with the same name */
    pthread_mutex_lock(&session_lock);
    /* Loop to find existing session slot */
    for (int i = 0; i < MAX_SESSION; i++) {
        if (session_list[i].user_count != 0){
            if (strcmp(session_list[i].id, data) == 0){
                pthread_mutex_unlock(&session_lock);
                return 2;
            }
        }
    }

    /* Loop to find empty session slot */
    for (int i = 0; i < MAX_SESSION; i++) {
        if (session_list[i].user_count == 0){
            /* Create a session with given id */
            strcpy(session_list[i].id, data);
            session_list[i].user_count += 1;
            
            /* join session */
            strcpy(session_list[i].user_id_list[0], id);
            session_list[i].socketfd_list[0] = socketfd;

            pthread_mutex_lock(&user_lock);
            user_list[index].in_session = true;
            strcpy(user_list[index].session_id, data);
            pthread_mutex_unlock(&user_lock);

            fprintf(stdout, "Session [%s] created! \n", data);
            pthread_mutex_unlock(&session_lock);
            return 0;
        }
    }
    fprintf(stdout, "ERROR: Session is Full - cannot create session! \n");
    pthread_mutex_unlock(&session_lock);
    return 3;
}

char* generate_list(){
    // Allocate memory for the output string
    int output_size = 0;
    bool has_session = false;

    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < MAX_USER; i++) {
        if (user_list[i].connected == true)
            output_size += strlen(user_list[i].id) + 2;  // Add 2 for the prefix "\t"
    }
    pthread_mutex_unlock(&user_lock);

    pthread_mutex_lock(&session_lock);
    for (int i = 0; i < MAX_SESSION; i++) {
        if (session_list[i].user_count != 0){
            output_size += strlen(session_list[i].id) + 2;  // Add 2 for the prefix "\t"
            has_session = true;
        }
    }
    pthread_mutex_unlock(&session_lock);

    // Add extra space for the prefix and suffix strings
    output_size += 28;  
    output_size += 31;
    char* output = (char*)malloc(output_size * sizeof(char));

    // Generate the output string
    pthread_mutex_lock(&user_lock);
    sprintf(output, "Here is the list of users:\n");
    for (int i = 0; i < MAX_USER; i++) {
        if (user_list[i].connected == true){
            strcat(output, "\t");
            strcat(output, user_list[i].id);
            strcat(output, "\n");
        }
    }
    pthread_mutex_unlock(&user_lock);

    pthread_mutex_lock(&session_lock);
    if (has_session){
        strcat(output, "Here is the list of sessions:\n");
        for (int i = 0; i < MAX_SESSION; i++) {
            if (session_list[i].user_count != 0){
                strcat(output, "\t");
                strcat(output, session_list[i].id);
                strcat(output, "\n");
            }
        }
    }
    else {
        strcat(output, "There is no existing session.\n");
    }
    pthread_mutex_unlock(&session_lock);

    return output;
}

int join_session(char data[], char id[], int* sockfd){
    /* Find index of user id in list */
    int index;
    int socketfd = *sockfd;
    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < MAX_USER; i++){
        if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
            index = i;
            break;
        }
    }
    pthread_mutex_unlock(&user_lock);

    /* Find the session with given id */
    /* Loop to find session */
    pthread_mutex_lock(&session_lock);
    for (int i = 0; i < MAX_SESSION; i++) {
        if (session_list[i].user_count != 0){
            /* found the session -> add user in */
            if (strcmp(session_list[i].id, data) == 0){
                /* ERROR: session is full */
                if (session_list[i].user_count == MAX_USER){
                    pthread_mutex_unlock(&session_lock);
                    return 1;
                }
                /* set session_list[i] and its user list */
                session_list[i].user_count += 1;
                for (int j = 0; j < MAX_USER; j++){
                    if (strlen(session_list[i].user_id_list[j]) == 0){ // empty slot for user in session
                        strcpy(session_list[i].user_id_list[j], id);
                        session_list[i].socketfd_list[j] = socketfd;
                        break;
                    }
                }
                /* set user list [index] */
                pthread_mutex_lock(&user_lock);
                user_list[index].in_session = true;
                strcpy(user_list[index].session_id, data);
                pthread_mutex_unlock(&user_lock);

                pthread_mutex_unlock(&session_lock);
                fprintf(stdout, "User [%s] joins Session [%s].\n", id, data);
                return 0;
            }
        }
    }
    /* Session does not exist */
    pthread_mutex_unlock(&session_lock);
    return 2;
}

void broadcast(char data[], char id[]){
    /* modify the format of output message */
    /* [MSG from user_1]: this is message */
    char temp[BUFF_SIZE];
    memset(temp, 0, BUFF_SIZE);
    strcpy(temp, "[MSG from ");
    strcat(temp, id);
    strcat(temp, "]: ");
    strcat(temp, data);

    /* Create MSG packet to users */
    packet send_pkt;
    send_pkt.type = 11; // MESSAGE
    send_pkt.size = strlen(temp);
    strcpy(send_pkt.data, temp);

    char buf[BUFF_SIZE];
    memset(buf, 0, BUFF_SIZE);
    createPacket(&send_pkt, buf);

    /* Find the session id for broadcast */
    char session_id[MAX_DATA];
    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < MAX_USER; i++){
        if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
            strcpy(session_id, user_list[i].session_id);
            break;
        }
    }
    pthread_mutex_unlock(&user_lock);

    /* Loop to find session */
    pthread_mutex_lock(&session_lock);
    for (int i = 0; i < MAX_SESSION; i++) {
        if (session_list[i].user_count != 0){
            if (strcmp(session_list[i].id, session_id) == 0){ // found the session
                /* broadcast to all its users in user list */
                for (int j = 0; j < MAX_USER; j++){
                    if (strlen(session_list[i].user_id_list[j]) != 0 && 
                        strcmp(session_list[i].user_id_list[j], id) != 0){ // other available users
                        /* Send MSG to server */
                        if (send(session_list[i].socketfd_list[j], buf, BUFF_SIZE, 0) == -1){
                            fprintf(stdout, "ERROR: server broadcast message - send error. \n");
                            pthread_mutex_unlock(&session_lock);
                            return;
                        }
                    }
                }
                break;
            }
        }
    }
    pthread_mutex_unlock(&session_lock);
}

void *server_func(void *arg) {
    int *socketfd = arg;
    char buffer[BUFF_SIZE];
    char *cursor;
    char id[MAX_NAME], data[MAX_DATA];

    while (true) {
        int byte_recv;
        fd_set read_fds;
        struct timeval tv;
        packet recv_pkt, send_pkt;

        memset(&buffer, 0, BUFF_SIZE);
        memset(&data, 0, MAX_DATA);

        /* Set Timeout Option for Recv */
        FD_ZERO(&read_fds);
        FD_SET(*socketfd, &read_fds);

        // set timeout to 60 seconds
        tv.tv_sec = 60;
        tv.tv_usec = 0;

        // wait for socketfd to become readable or timeout
        int rv = select((*socketfd)+1, &read_fds, NULL, NULL, &tv);
        if (rv == -1) {
            fprintf(stdout, "ERROR: server Select failed. \n");
            continue;
        }
        else if (rv == 0) {
            /* check if user is in session */
            bool is_in_session = false;
            pthread_mutex_lock(&user_lock);
            for (int i = 0; i < MAX_USER; i++){
                if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
                    // leave session before logout
                    if (user_list[i].in_session){
                        is_in_session = true;
                    } 
                    break;
                }
            }
            pthread_mutex_unlock(&user_lock);

            /* delete user when logout */
            if (is_in_session) leave_session(id);
            delete_user(id);

            /* Send TIMEOUT pkt to client for timeout */
            send_pkt.type = 18; // TIMEOUT
            strcpy(send_pkt.source, id);
            strcpy(send_pkt.data, "ERROR: Timeout - Logging out. \n");
            send_pkt.size = strlen(send_pkt.data);

            /* Sending Packets to client */
            memset(&buffer, 0, BUFF_SIZE);
            createPacket(&send_pkt, buffer);
            if (send(*socketfd, buffer, BUFF_SIZE, 0) == -1){
                fprintf(stdout, "ERROR: server func - send error. \n");
                close(*socketfd);
                pthread_exit(NULL);
            }

            close(*socketfd);
            pthread_exit(NULL);
            break;
        }

        /* Receiving from socket */
        byte_recv = recv(*socketfd, buffer, BUFF_SIZE, 0);
        if(byte_recv == -1) {
            fprintf(stdout, "ERROR: server func recv. \n");
            close(*socketfd);
            pthread_exit(NULL);
        }
        else if (byte_recv == 0){
            /* check if user is in session */
            bool is_in_session = false;
            bool is_connected = false;

            pthread_mutex_lock(&user_lock);
            for (int i = 0; i < MAX_USER; i++){
                if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
                    is_connected = true;
                    // leave session before logout
                    if (user_list[i].in_session){
                        is_in_session = true;
                    } 
                    break;
                }
            }
            pthread_mutex_unlock(&user_lock);

            /* delete user when logout */
            if (is_in_session) leave_session(id);
            if (is_connected) delete_user(id);

            fprintf(stdout, "0 bytes recv, a connection is closed \n");

            close(*socketfd);
            pthread_exit(NULL);
        }
        
        /* login request */
        readPacket(&recv_pkt, buffer);
        if (recv_pkt.type == 1){ // LOGIN
            /* Store id and password */
            strncpy(id, (char *)(recv_pkt.source), MAX_NAME);
            strncpy(data, (char *)(recv_pkt.data), MAX_DATA);
            
            /* Check if ID exists in user list */
            pthread_mutex_lock(&user_lock);
            bool dup_user = false;
            for (int i = 0; i < MAX_USER; i++){
                if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
                    dup_user = true;
                    break;
                }
            }
            pthread_mutex_unlock(&user_lock);

            int auth = authentication("userlist_db.txt", id, data);

            /* Send-packet Creation */
            /* NAK for duplicate user_id */
            if (dup_user){
                send_pkt.type = 3; // LO_NAK
                strcpy(send_pkt.source, id);
                strcpy(send_pkt.data, "LO_NAK: Login - User already logged in!\n");
                send_pkt.size = strlen(send_pkt.data);
                memset(&id, 0, MAX_NAME);
            }
            /* NAK for wrong pwd */
            else if (auth == 1){
                send_pkt.type = 3; // LO_NAK
                strcpy(send_pkt.source, id);
                strcpy(send_pkt.data, "LO_NAK: Login - Wrong password!\n");
                send_pkt.size = strlen(send_pkt.data);
                memset(&id, 0, MAX_NAME);
            }
            /* NAK for username that is not registered*/
            else if (auth == 2){
                send_pkt.type = 3; // LO_NAK
                strcpy(send_pkt.source, id);
                strcpy(send_pkt.data, "LO_NAK: Login - Username not registered or Userlist is missing.\n");
                send_pkt.size = strlen(send_pkt.data);
                memset(&id, 0, MAX_NAME);
            }
            /* else go ACK msg */
            else {
                send_pkt.type = 2; // LO_ACK
                strcpy(send_pkt.source, id);
                send_pkt.size = 0;
                add_user(id, data, socketfd);
            }
            
            /* Sending Packets to client */
            memset(&buffer, 0, BUFF_SIZE);
            createPacket(&send_pkt, buffer);
            if (send(*socketfd, buffer, BUFF_SIZE, 0) == -1){
                fprintf(stdout, "ERROR: server func - send error. \n");
                close(*socketfd);
                pthread_exit(NULL);
            }
        }
        else if (recv_pkt.type == 4) { // EXIT
            /* check if user is in session */
            bool is_in_session = false;
            pthread_mutex_lock(&user_lock);
            for (int i = 0; i < MAX_USER; i++){
                if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
                    // leave session before logout
                    if (user_list[i].in_session){
                        is_in_session = true;
                    } 
                    break;
                }
            }
            pthread_mutex_unlock(&user_lock);

            /* delete user when logout */
            if (is_in_session) leave_session(id);
            delete_user(id);

            close(*socketfd);
            pthread_exit(NULL);
        }
        else if (recv_pkt.type == 5) { // JOIN
            /* Store id and join session */
            strncpy(data, (char *)(recv_pkt.data), MAX_DATA);
            int res = join_session(data, id, socketfd);

            /* Send-packet Creation */
            if (res == 0){
                send_pkt.type = 6; // JN_ACK
                send_pkt.size = 0;
            } 
            else if (res == 1){ // session full
                send_pkt.type = 7; // JN_NAK
                strcpy(send_pkt.data, "JN_NAK: Session [");
                strcat(send_pkt.data, data);
                strcat(send_pkt.data, "] is Full.\n");
                send_pkt.size = strlen(send_pkt.data);
            }
            else if (res == 2){ // session not exist
                send_pkt.type = 7; // JN_NAK
                char* temp;
                strcpy(send_pkt.data, "JN_NAK: Session [");
                strcat(send_pkt.data, data);
                strcat(send_pkt.data, "] does not exist.\n");
                send_pkt.size = strlen(send_pkt.data);
            }

            /* Sending Packets to client */
            memset(&buffer, 0, BUFF_SIZE);
            createPacket(&send_pkt, buffer);
            if (send(*socketfd, buffer, BUFF_SIZE, 0) == -1){
                fprintf(stdout, "ERROR: server func - NS_ACK error. \n");
            }
        }
        else if (recv_pkt.type == 8) { // LEAVE_SESS
            leave_session(id);
        }
        else if (recv_pkt.type == 9) { // NEW_SESS
            /* Store id and create session */
            strncpy(data, (char *)(recv_pkt.data), MAX_DATA);
            int res = create_session(data, id, socketfd);

            /* Send-packet Creation */
            if (res == 0){
                send_pkt.type = 10; // NS_ACK
                send_pkt.size = 0;
            } 
            else if (res == 1){ // user in session
                send_pkt.type = 14; // NS_NAK
                strcpy(send_pkt.data, "NS_NAK: User already in session.\n");
                send_pkt.size = strlen(send_pkt.data);
            }
            else if (res == 3){ // session full
                send_pkt.type = 14; // NS_NAK
                strcpy(send_pkt.data, "NS_NAK: Too many Sessions.\n");
                send_pkt.size = strlen(send_pkt.data);
            }
            else if (res == 2){ // session exists
                send_pkt.type = 14; // NS_NAK
                strcpy(send_pkt.data, "NS_NAK: Session name exists, try another name.\n");
                send_pkt.size = strlen(send_pkt.data);
            }

            /* Sending Packets to client */
            memset(&buffer, 0, BUFF_SIZE);
            createPacket(&send_pkt, buffer);
            if (send(*socketfd, buffer, BUFF_SIZE, 0) == -1){
                fprintf(stdout, "ERROR: server func - NS_ACK error. \n");
            }
        }
        else if (recv_pkt.type == 12) { // QUERY
            /* Send-packet Creation */
            send_pkt.type = 13; // QU_ACK
            char* temp = generate_list();
            memset(&send_pkt.data, 0, send_pkt.size);
            strcpy(send_pkt.data, temp);
            send_pkt.size = strlen(send_pkt.data);

            /* Sending Packets to client */
            memset(&buffer, 0, BUFF_SIZE);
            createPacket(&send_pkt, buffer);
            if (send(*socketfd, buffer, BUFF_SIZE, 0) == -1){
                fprintf(stdout, "ERROR: server func - QU_ACK error. \n");
            }
        }
        else if (recv_pkt.type == 11) { // MESSAGE
            broadcast(recv_pkt.data, id);
        }
        else if (recv_pkt.type == 15) { // REGISTER
            /* Store id and password */
            strncpy(id, (char *)(recv_pkt.source), MAX_NAME);
            strncpy(data, (char *)(recv_pkt.data), MAX_DATA);

            /* Packet Sending */
            /* register successfully -> send ACK */
            if (register_user("userlist_db.txt", id, data) == true){
                send_pkt.type = 16; // REG_ACK
                strcpy(send_pkt.source, id);
                send_pkt.size = 0;
            }
            /* else, send NAK for error reasoning */
            else {
                send_pkt.type = 17; // REG_NAK
                strcpy(send_pkt.source, id);
                strcpy(send_pkt.data, "REG_NAK: Register failed, username exists!\n");
                send_pkt.size = strlen(send_pkt.data);
            }
            
            /* Sending Packets to client */
            memset(&buffer, 0, BUFF_SIZE);
            createPacket(&send_pkt, buffer);
            if (send(*socketfd, buffer, BUFF_SIZE, 0) == -1){
                fprintf(stdout, "ERROR: server func - send error. \n");
                close(*socketfd);
                pthread_exit(NULL);
            }
        }
        else
            fprintf(stdout, "ERROR: server func - receive unexpected ACK. \n");
    } 
}

int main (int argc, char *argv[]) {
    if (argc != 2){
        printf("Usage: server <TCP listen port> \n");
        return -1;
    }

    /* Variable Init */
    int socketfd;
    bool connected = false;
    char* port = argv[1]; // int port = atoi(argv[1]);
    int yes = 1;

    struct addrinfo hint, *res;
    memset(&hint, 0, sizeof hint);
    hint.ai_family = AF_INET; // IPv4  
    hint.ai_socktype = SOCK_STREAM; // for TCP
    hint.ai_flags = AI_PASSIVE;     // auto fill ip

    /* Loop over user list and set default false bool */
    for (int i = 0; i < MAX_USER; i++){
        user_list[i].connected = false;
        user_list[i].in_session = false;
    }

    /* Loop over session list and set user count to 0 */
    for (int i = 0; i < MAX_SESSION; i++){
        session_list[i].user_count = 0;
    }

    /* Connection to port */
    if (getaddrinfo(NULL, port, &hint, &res) != 0){
        fprintf(stdout, "ERROR: server getaddrinfo. \n");
        return -1;
    }

    /* Loop over res to find available connection */
    for(struct addrinfo *temp = res; temp != NULL; temp = temp->ai_next) {
        socketfd = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol);
        if (socketfd == -1) {
            fprintf(stdout, "ERROR: server socket. \n");
            continue;
        }

        /* By setting the SO_REUSEADDR option, the program can bind to the port 
        without waiting for the previous connection to time out. */
        if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            close(socketfd);
            fprintf(stdout, "ERROR: server setsocketopt. \n");
            continue;
        }

        if (bind(socketfd, temp->ai_addr, temp->ai_addrlen) == -1) {
            close(socketfd);
            fprintf(stdout, "ERROR: server bind. \n");
            continue;
        }

        connected = true;
        break; 
    }

    /* if no available connection in res */
    if (connected == false){
        fprintf(stdout, "ERROR: No available connection in res. \n");
        return -1;
    }

    /* TCP Listen on Port */
    if (listen(socketfd, QUEUE_SIZE) == -1) {
        fprintf(stdout, "ERROR: server listen. \n");
        return -1;
    }

    // loop for accepting messages
    while (true) {
        struct sockaddr_storage addr;
        socklen_t len = sizeof(addr);
        int* new_socketfd = malloc(sizeof(int)); 
        *new_socketfd = accept(socketfd, (struct sockaddr *)&addr, &len);
        if (*new_socketfd == -1) {
            fprintf(stdout, "ERROR: server accept. \n");
            continue;
        }
        pthread_t thread;
        pthread_create(&thread, NULL, server_func, (void *)new_socketfd);
    }
    
    return 0;
}