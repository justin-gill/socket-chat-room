/* server.c - code for server program. Do not rename this file */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#include "proj.h"
#include "p4.h"

#define QLEN 6 //
int visits = 0; /* counts client connections */

struct client{
    int sd;
    char username[11];
    int status;
    time_t start_time;
    time_t timeout_time;
    int client_frg;
    int client_lst;
    char message[10000];
    int fragments_sent;
};

void handle_non_fragmented(uint16_t *control_header, uint8_t *ulen, char *message, struct client clients[], int *i,
int *max_clients, uint32_t *message_header, uint8_t *pub, uint8_t *prv, uint16_t *len, char* username, int *sent_message);
void handle_fragmented(uint16_t *control_header, uint8_t *ulen, char *message, struct client clients[], int *i,
int *max_clients, uint32_t *message_header, uint8_t *pub, uint8_t *prv, uint8_t *lst, uint16_t *len, char* username,
int *sent_message, char* temp_message);
void handle_user_negotiation(uint16_t *control_header, uint8_t *ulen, uint8_t *max_ulen, uint8_t *username_length,
int *sd, char *message, struct client clients[], int *i, int *valid_username, int *max_clients, int *timeout);

/**
 * @brief This is the main driver function of the server
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(int argc, char **argv) {
    char temp_message[255];
    int valid_username = 0;
    uint8_t username_length;
    int num_of_fds = 0;
    struct protoent *ptrp;
    struct sockaddr_in sad;
    struct sockaddr_in cad;
    int sd, listensd;
    int port;
    int optval = 1;
    int max_clients;
    char message[255];
    int timeout;
    // shared
    uint8_t mt = 0x0;
    uint8_t ulen = 0x0; 
    // control message
    uint8_t max_ulen;
    uint16_t control_header = 0x0;    
    uint8_t code = 0x0; 
    uint8_t unc = 0x0; 
    // message header
    uint32_t message_header = 0x0;
    uint8_t pub = 0x0;
    uint8_t prv = 0x0;
    uint8_t frg = 0x0;
    uint8_t lst = 0x0;
    uint16_t len = 0x0;
    int n;
    char username[10];
    int sent_message;

    if (argc != 5) {
        fprintf(stderr, "Error: Wrong number of arguments\n");
        fprintf(stderr, "usage:\n");
        fprintf(stderr, "./server PORT MAX_CLIENTS TIMEOUT MAX_ULEN\n");
        exit(EXIT_FAILURE);
    }

    memset((char *) &sad, 0, sizeof(sad)); /* clear sockaddr structure */
    sad.sin_family = AF_INET; /* set family to Internet */
    sad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */


    port = atoi(argv[1]); /* convert argument to binary */
    if (port > 0) { /* test for illegal value */
        sad.sin_port = htons((u_short) port);
    }else { /* print error message and exit */
        perror("Error: bad port number");
        exit(EXIT_FAILURE);
    }
    max_clients = atoi(argv[2]);
    if (max_clients < 1 || max_clients > 255){
        fprintf(stderr, "Invalid value for MAX_CLIENTS\n");
        exit(EXIT_FAILURE);
    }
    timeout = atoi(argv[3]);
    if (timeout < 1 || timeout > 299){
        fprintf(stderr, "Invalid value for TIMEOUT\n");
        exit(EXIT_FAILURE);
    }
    max_ulen = atoi(argv[4]);
    if (max_ulen < 1 || max_ulen > 10){
        fprintf(stderr, "Invalid value for MAX_ULEN\n");
        exit(EXIT_FAILURE);
    }
    struct timeval tv; 
    // set up clients struct
    struct client clients[max_clients];
    for (int i = 0; i < max_clients; i++){
        clients[i].sd = 0;
        clients[i].status = 0;
        clients[i].fragments_sent = 0;
    }

    /* Map TCP transport protocol name to protocol number */
    if (((long int) (ptrp = getprotobyname("tcp"))) == 0) {
        fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
        exit(EXIT_FAILURE);
    }

    /* Create a socket */
    listensd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
    if (listensd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }


    /* Allow reuse of port - avoid "Bind failed" issues */
    if (setsockopt(listensd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("Error setting socket option");
        exit(EXIT_FAILURE);
    }

    /* Bind a local address to the socket */
    if (bind(listensd, (struct sockaddr *) &sad, sizeof(sad)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }


    /* Specify size of request queue */
    if (listen(listensd, QLEN) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    fd_set read_set, ready_to_read_set;
    int max_fd;

    FD_ZERO(&read_set);
    FD_SET(listensd, &read_set);
    max_fd = listensd;

    while (1) {
        // reset variables
        memset(temp_message, 0, strlen(temp_message));
        bzero(message, sizeof(message));
        control_header = 0;
        memcpy(&ready_to_read_set, &read_set, sizeof(read_set));
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        int r = select(max_fd + 1, &ready_to_read_set, NULL, NULL, &tv);
        if (r < 0) {
            perror("Error with select");
            exit(EXIT_FAILURE);
        }else if (r == 0) {
            // check each user's timeout
            for(int i = 0; i < max_clients; i++){
                if (clients[i].status == 1){
                    // if the user timeouts, send control header and close socket
                    if(clients[i].timeout_time >= time(NULL)){
                        create_control_header(&control_header, 0, 4, 0, max_ulen);
                        control_header = htons(control_header);
                        send(sd, &control_header, sizeof(control_header), 0);
                        close(clients[i].sd);
                        FD_CLR(clients[i].sd, &read_set);
                        // reset the user attributes
                        clients[i].sd = 0;
                        clients[i].status = 0;
                        num_of_fds--;
                    }
                }
            }
        }else{
            // check for new client
            if (FD_ISSET(listensd, &ready_to_read_set)) {
                socklen_t alen = sizeof(cad);
                if ((sd = accept(listensd, (struct sockaddr *) &cad, &alen)) < 0) {
                    fprintf(stderr, "Error: Accept failed\n");
                    exit(EXIT_FAILURE);
                }
                // if the server is full
                if(num_of_fds >= max_clients){
                    // printf("Sending server is full\n");
                    create_control_header(&control_header, 0, 0, 0, 0);
                    // send the first header to client
                    control_header = htons(control_header);
                    send(sd, &control_header, sizeof(control_header), 0);
                    // close the socket
                    close(sd);
                // if the server is not full, put client into fd list
                }else{
                    for(int i = 0; i < max_clients; i++){
                        if (clients[i].sd == 0){
                            clients[i].sd = sd;
                            FD_SET(clients[i].sd, &read_set);
                            clients[i].status = 1;
                            max_fd = clients[i].sd > max_fd ? clients[i].sd : max_fd;
                            num_of_fds++;
                            clients[i].start_time = time(NULL);
                            clients[i].timeout_time = clients[i].start_time + timeout;
                            break;
                        }
                    }
                    // send server is not full
                    create_control_header(&control_header, 0, 1, 0, max_ulen);
                    control_header = htons(control_header);
                    send(sd, &control_header, sizeof(control_header), 0);
                    // search for first open fd slot
                }
            }
            // iterate over all clients
            for(int i = 0; i < max_clients; i++){
                // if the user is in use
                if (clients[i].sd != 0){
                    // if the client is set
                    if (FD_ISSET(clients[i].sd, &ready_to_read_set)){
                        // recv the first header as 16 bits in the control header
                        n = recv(clients[i].sd, &control_header, sizeof(control_header), 0); 
                        // check to see if control header or message header
                        // user negotiation, user status 1 is user negotiation
                        if(clients[i].status == 1){
                                control_header = ntohs(control_header);
                                parse_control_header(&control_header, &mt, &code, &unc, &username_length);
                                // receive the header for the username
                                int n = recv(sd, message, username_length, 0);
                                // if the received message errors, disconnect the user
                                if (n <= 0) {
                                    if (n < 0) {
                                        perror("Error receiving data from client");
                                    }
                                    num_of_fds--;
                                    close(clients[i].sd);
                                    FD_CLR(clients[i].sd, &read_set); // clear this
                                    clients[i].sd = 0;
                                    clients[i].status = 0;
                                }else{
                                    handle_user_negotiation(&control_header, &ulen, &max_ulen, &username_length, 
                                    &sd, message, clients, &i, &valid_username, &max_clients, &timeout);
                                }
                        }else if (clients[i].status == 2){
                            // when the user is active
                            // if the message received errors, disconnect the user
                            if (n <= 0) {
                                if (n < 0) {
                                    perror("Error receiving data from client");
                                }
                                num_of_fds--;
                                close(clients[i].sd);
                                FD_CLR(clients[i].sd, &read_set); 
                                clients[i].sd = 0;
                                clients[i].status = 0;
                                for(int j = 0; j < max_clients; j++){
                                    if(clients[j].sd != 0){                   
                                        create_control_header(&control_header, 0, 3, 0, strlen(clients[i].username));
                                        control_header = htons(control_header);
                                        strcpy(message, clients[i].username);
                                        if (clients[j].status == 2){
                                            send(clients[j].sd, &control_header, sizeof(control_header), 0);
                                            send(clients[j].sd, message, strlen(message), 0);
                                        }     
                                    }
                                }
                                // reset username so it is available
                                memset(clients[i].username, 0, sizeof(clients[i].username));
                                clients[i].username[0] = '\0';
                            }else{
                                sent_message = 0;
                                // recv the other 16 bytes from the server
                                n = recv(clients[i].sd, &message_header, 2, 0);
                                // // bit shift the just recv'd bytes and append the previously recv'd 16 bytes
                                message_header = message_header << 16;
                                message_header += control_header;     
                                message_header = ntohl(message_header);
                                // print_u32_as_bits(message_header);
                                // receive the username and message
                                parse_chat_header(&message_header, &mt, &pub, &prv, &frg, &lst, &ulen, &len);
                                if (frg){
                                    handle_fragmented(&control_header, &ulen, message, clients, &i, &max_clients, 
                                    &message_header, &pub, &prv, &lst, &len, username, &sent_message, temp_message);

                                }else{
                                    handle_non_fragmented(&control_header, &ulen, message, clients,
                                    &i, &max_clients, &message_header, &pub, &prv, &len, username, &sent_message);
                                }
                            }
                        }
                    }
                }            
            }
        }        
    }
    return 0;
}


/**
 * @brief This function handles the user negotiation stage 
 * 
 * @param control_header 
 * @param ulen 
 * @param max_ulen 
 * @param username_length 
 * @param sd 
 * @param message 
 * @param clients 
 * @param i 
 * @param valid_username 
 * @param max_clients 
 * @param timeout 
 */
void handle_user_negotiation(uint16_t *control_header, uint8_t *ulen, uint8_t *max_ulen, uint8_t *username_length,
int *sd, char *message, struct client clients[], int *i, int *valid_username, int *max_clients, int *timeout){
    // check usernames
    *valid_username = 1;
    for(int j = 0; j < *max_clients; j++){
        if (clients[j].sd != 0){
            if (strcmp(message, clients[j].username) == 0){
                *valid_username = 0;
                create_control_header(control_header, 0, 4, 1, 0);
                *control_header = htons(*control_header);
                send(*sd, control_header, sizeof(*control_header), 0);
                break;
            }
        }
    }
    if(*valid_username == 1){
        // username longer than max length
        if (strlen(message) > *max_ulen){
            *valid_username = 0;
            create_control_header(control_header, 0, 4, 2, 0);
            *control_header = htons(*control_header);
            send(*sd, control_header, sizeof(*control_header), 0);
        }
    }
    if (*valid_username == 1){
        // check valid username
        for(int j = 0; j < strlen(message); j++){
            if (!(((int)message[j] >= 48 && (int)message[j] <= 57) || 
            ((int)message[j] >= 65 && (int)message[j] <= 90) || 
            ((int)message[j] >= 97 && (int)message[j] <= 122))){
                *valid_username = 0;
                create_control_header(control_header, 0, 4, 3, 0);
                *control_header = htons(*control_header);
                send(*sd, control_header, sizeof(*control_header), 0);
                break;
            }
        }  
    }
    if(!*valid_username){
        clients[*i].start_time = time(NULL);
        clients[*i].timeout_time = clients[*i].start_time + *timeout;
    }
    // if the username was valid
    if (*valid_username == 1){
        strcpy(clients[*i].username, message);
        *username_length = strlen(message);
        create_control_header(control_header, 0, 4, 4, *username_length);
        *control_header = htons(*control_header);
        send(*sd, control_header, sizeof(*control_header), 0);
        send(*sd, message, *username_length, 0);
        clients[*i].status = 2;
        // send accepted user to all current users, including user that just connected
        for(int j = 0; j < *max_clients; j++){
            if(clients[j].sd != 0){                      
                create_control_header(control_header, 0, 2, 0, *username_length);
                *control_header = htons(*control_header);
                send(clients[j].sd, control_header, sizeof(*control_header), 0);
                send(clients[j].sd, message, strlen(message), 0);
            }
        }
    }
}

/**
 * @brief This function handles the fragmented messages that are received and sent to the client(s)
 * 
 * @param control_header 
 * @param ulen 
 * @param message 
 * @param clients 
 * @param i 
 * @param max_clients 
 * @param message_header 
 * @param pub 
 * @param prv 
 * @param lst 
 * @param len 
 * @param username 
 * @param sent_message 
 * @param temp_message 
 */
void handle_fragmented(uint16_t *control_header, uint8_t *ulen, char *message, struct client clients[], int *i,
int *max_clients, uint32_t *message_header, uint8_t *pub, uint8_t *prv, uint8_t *lst, uint16_t *len, char* username,
int *sent_message, char* temp_message){
    if (*prv){
        // if there is no username, send control header with code 7
        if (*ulen == 0){
            recv(clients[*i].sd, username, *ulen, 0);
            recv(clients[*i].sd, clients[*i].message, *len, 0);
            create_control_header(control_header, 0, 7, 0, strlen(username));
            *control_header = htons(*control_header);
            send(clients[*i].sd, control_header, sizeof(*control_header), 0);
            return;
        }
        // receive the username of the user to send it to and the message
        recv(clients[*i].sd, username, *ulen, 0);
        // read the message from the last place message ended
        recv(clients[*i].sd, temp_message, *len, 0);
        if (*lst){
            temp_message[(clients[*i].fragments_sent * 255) + *len] = '\0';
        }
        strcat(clients[*i].message, temp_message);
        // clients[i].message[len] = '\0';
        username[*ulen] = '\0';
        // if this is the last message and me message sent is not > than the max bytes
        if (*lst){
            // set the last char to \0 to end string
            clients[*i].message[(clients[*i].fragments_sent * 255) + *len] = '\0';
            // if the message is greater than the max length
            if (strlen(clients[*i].message) > 4095){
                create_control_header(control_header, 0, 5, 0, 0);
                *control_header = htons(*control_header);
                send(clients[*i].sd, control_header, sizeof(*control_header), 0);
                memset(clients[*i].message, 0, strlen(clients[*i].message));
                memset(temp_message, 0, strlen(temp_message));
                *sent_message = 1;
            }else{
                for(int j = 0; j < *max_clients; j++){
                    // if the client is active
                    if (clients[j].sd != 0){
                        // if the client has the matching username
                        if (strcmp(clients[j].username, username) == 0){
                            clients[*i].fragments_sent = 0;
                            memset(temp_message, 0, strlen(temp_message));
                            *sent_message = 1;
                            create_chat_header(message_header, 1, 0, 1, 0, 0, strlen(clients[*i].username), strlen(clients[*i].message));
                            // print_u32_as_bits(message_header);
                            *message_header = htonl(*message_header);
                            send(clients[j].sd, message_header, sizeof(*message_header), 0);
                            // send the senders username to the receiver
                            send(clients[j].sd, clients[*i].username, strlen(clients[*i].username), 0);
                            // send the senders message to the receiver
                            send(clients[j].sd, clients[*i].message, strlen(clients[*i].message), 0);
                            memset(clients[*i].message, 0, strlen(clients[*i].message));
                        }
                    }
                }
            }
            // if the username did not exist     
            if (!*sent_message){
                // if the message is not sent, increment that # of fragments sent
                create_control_header(control_header, 0, 6, 0, strlen(username));
                *control_header = htons(*control_header);
                send(clients[*i].sd, control_header, sizeof(*control_header), 0);
                send(clients[*i].sd, username, strlen(username), 0);
                memset(clients[*i].message, 0, strlen(clients[*i].message));
                memset(temp_message, 0, strlen(temp_message));
            }
        }
        if (!*lst){
            clients[*i].fragments_sent++;
        }    
    }else if (*pub){
        recv(clients[*i].sd, temp_message, *len, 0);
        strcat(clients[*i].message, temp_message);
        // if this is the last fragment
        if (*lst){
            // append the \0 char to end string
            clients[*i].message[(clients[*i].fragments_sent * 255) + *len] = '\0';
            // reset the fragments sent for the next iteration
            clients[*i].fragments_sent = 0;
            memset(temp_message, 0, strlen(temp_message));
            // if the message was longer, disregard message and send code 5 to user
            if (strlen(clients[*i].message) > 4095){
                create_control_header(control_header, 0, 5, 0, strlen(username));
                *control_header = htons(*control_header);
                send(clients[*i].sd, control_header, sizeof(*control_header), 0);
                memset(clients[*i].message, 0, strlen(clients[*i].message));
            }else{
                for (int j = 0; j < *max_clients; j++){
                    if (clients[j].sd != 0 && clients[j].sd != clients[*i].sd){
                        create_chat_header(message_header, 1, 1, 0, 0, 0, strlen(clients[*i].username), strlen(clients[*i].message));
                        // print_u32_as_bits(message_header);
                        *message_header = htonl(*message_header);
                        send(clients[j].sd, message_header, sizeof(*message_header), 0);
                        // send the senders username to the receiver
                        send(clients[j].sd, clients[*i].username, strlen(clients[*i].username), 0);
                        // send the senders message to the receiver
                        send(clients[j].sd, clients[*i].message, strlen(clients[*i].message), 0);
                        memset(clients[*i].message, 0, strlen(clients[*i].message));
                    }
                }
            }                                            
        }
        if (!*lst){
            clients[*i].fragments_sent++;
        }    
    }
}

/**
 * @brief This function handles the non fragmented messages and sends/receives them to/from the client(s)
 * 
 * @param control_header 
 * @param ulen 
 * @param message 
 * @param clients 
 * @param i 
 * @param max_clients 
 * @param message_header 
 * @param pub 
 * @param prv 
 * @param len 
 * @param username 
 * @param sent_message 
 */
void handle_non_fragmented(uint16_t *control_header, uint8_t *ulen, char *message, struct client clients[], int *i,
int *max_clients, uint32_t *message_header, uint8_t *pub, uint8_t *prv, uint16_t *len, char* username, int *sent_message){
    if (*len > 255){
        recv(clients[*i].sd, username, *ulen, 0);
        recv(clients[*i].sd, clients[*i].message, *len, 0);
        create_control_header(control_header, 0, 5, 0, strlen(username));
        *control_header = htons(*control_header);
        send(clients[*i].sd, control_header, sizeof(*control_header), 0);
        return;
    }
    if (*prv){
        if (*ulen == 0){
            recv(clients[*i].sd, username, *ulen, 0);
            recv(clients[*i].sd, clients[*i].message, *len, 0);
            create_control_header(control_header, 0, 7, 0, strlen(username));
            *control_header = htons(*control_header);
            send(clients[*i].sd, control_header, sizeof(*control_header), 0);
            return;
        }
        // receive the username of the user to send it to and the message
        recv(clients[*i].sd, username, *ulen, 0);
        recv(clients[*i].sd, clients[*i].message, *len, 0);
        clients[*i].message[*len] = '\0';
        username[*ulen] = '\0';
        for(int j = 0; j < *max_clients; j++){
            // if the client is active
            if (clients[j].sd != 0){
                // if the client has the matching username
                if (strcmp(clients[j].username, username) == 0){
                    *sent_message = 1;
                    create_chat_header(message_header, 1, 0, 1, 0, 0, strlen(clients[*i].username), strlen(clients[*i].message));
                    // print_u32_as_bits(message_header);
                    *message_header = htonl(*message_header);
                    send(clients[j].sd, message_header, sizeof(*message_header), 0);
                    // send the senders username to the receiver
                    send(clients[j].sd, clients[*i].username, strlen(clients[*i].username), 0);
                    // send the senders message to the receiver
                    send(clients[j].sd, clients[*i].message, strlen(clients[*i].message), 0);
                    memset(clients[*i].message, 0, strlen(clients[*i].message));
                }
            }
        }
        if (!*sent_message){
            create_control_header(control_header, 0, 6, 0, strlen(username));
            *control_header = htons(*control_header);
            // sprintf(message, username);
            send(clients[*i].sd, control_header, sizeof(*control_header), 0);
            send(clients[*i].sd, username, strlen(username), 0);
            memset(clients[*i].message, 0, strlen(clients[*i].message));
        }
    }else if (*pub){
        recv(clients[*i].sd, clients[*i].message, *len, 0);
        clients[*i].message[*len] = '\0';
        for (int j = 0; j < *max_clients; j++){
            if (clients[j].sd != 0 && clients[j].sd != clients[*i].sd){
                create_chat_header(message_header, 1, 1, 0, 0, 0, strlen(clients[*i].username), strlen(clients[*i].message));
                // print_u32_as_bits(message_header);
                *message_header = htonl(*message_header);
                send(clients[j].sd, message_header, sizeof(*message_header), 0);
                // send the senders username to the receiver
                send(clients[j].sd, clients[*i].username, strlen(clients[*i].username), 0);
                // send the senders message to the receiver
                send(clients[j].sd, clients[*i].message, strlen(clients[*i].message), 0);
                memset(clients[*i].message, 0, strlen(clients[*i].message));
            }
        }
    }
}