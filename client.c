/* client.c - code for client program. Do not rename this file */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <math.h>

#include "proj.h"

void handle_send_message(char* input, int *more, char* to_username, char* to_message, int *message_length, uint32_t *message_header, int *sd);
void handle_control_header(uint16_t *control_header, uint8_t *mt, uint8_t *code, uint8_t *unc, uint8_t *max_ulen, int* input_max,
                            int* sd, char* message, int* username_accepted, char *client_username);

/**
 * @brief This function is the driver for the client
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main( int argc, char **argv) {
    struct hostent *ptrh;
    struct protoent *ptrp;
    struct sockaddr_in sad;
    int sd;
    int port;
    char *host;
    char buf[255];
    char input[5000];
    int more;
    fd_set read_set, ready_to_read_set;
    int username_accepted = 0;
    char *to_message;
    char *to_username = '\0';
    char from_message[5000];
    char from_username[11];
    uint32_t temp_message_header;
    char client_username[11];
    int message_length;

    memset((char *) &sad, 0, sizeof(sad)); /* clear sockaddr structure */
    sad.sin_family = AF_INET; /* set family to Internet */

    if (argc != 3) {
        fprintf(stderr, "Error: Wrong number of arguments\n");
        fprintf(stderr, "usage:\n");
        fprintf(stderr, "./client server_address server_port\n");
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[2]); /* convert to binary */
    if (port > 0) /* test for legal value */
        sad.sin_port = htons((u_short) port);
    else {
        perror("Error: bad port number");
        exit(EXIT_FAILURE);
    }

    host = argv[1]; /* if host argument specified */

    /* Convert host name to equivalent IP address and copy to sad. */
    ptrh = gethostbyname(host);
    if (ptrh == NULL) {
        perror("Error: Invalid host");
        exit(EXIT_FAILURE);
    }

    memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

    /* Map TCP transport protocol name to protocol number. */
    if (((long int) (ptrp = getprotobyname("tcp"))) == 0) {
        perror("Error: Cannot map \"tcp\" to protocol number");
        exit(EXIT_FAILURE);
    }

    /* Create a socket. */
    sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
    if (sd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    /* Connect the socket to the specified server. */
    if (connect(sd, (struct sockaddr *) &sad, sizeof(sad)) < 0) {
        perror("Connect failed");
        exit(EXIT_FAILURE);
    }
    // uint16_t control_header;
    // init all of the header field values
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
    // set the header and message in the buf
    // the header is the first two byte
    char message[255];
    FD_ZERO(&read_set);
    FD_SET(0, &read_set);
    FD_SET(sd, &read_set);
    int max_fd = sd;
    control_header = 0;
    int input_max;
    while (1){
        // bzero(to_message, sizeof(*to_message));
        control_header = 0;
        message_header = 0;
        temp_message_header = 0;
        bzero(message, sizeof(message));  
        bzero(from_message, sizeof(from_message));
        bzero(input, sizeof(input));
        to_message = NULL;
        bzero(from_username, sizeof(from_username));
        if(username_accepted){
            PRINT_MSG("> ");
        }
        memcpy(&ready_to_read_set, &read_set, sizeof(fd_set));
        int r = select(max_fd + 1, &ready_to_read_set, NULL, NULL, NULL);
        if (r < 0) {
            perror("Error with select");
            exit(EXIT_FAILURE);
        } else if (r == 0) {
            fprintf(stderr, "Select returned 0 even though timeout arg is NULL");
            continue;
        }
        // IF stdin is ready to read
        if(FD_ISSET(0, &ready_to_read_set)){
            if(username_accepted == 0){
                read_stdin(input, input_max+2, &more);
                if(input[0] == '\n'){
                    PRINT_wARG("Choose a username (should be less than %d): ", input_max + 1);
                    continue;
                }
                if (more){
                    while(more){
                        read_stdin(buf, 255, &more);
                    }
                    PRINT_wARG("Choose a username (should be less than %d): ", input_max + 1);
                }else{
                    create_control_header(&control_header, 0, 4, 0, strlen(input)-1);
                    strcpy(message, input);
                    // sprintf(message, input);
                    control_header = htons(control_header);
                    // send control header 
                    send(sd, &control_header, 2, 0);
                    // send the username 
                    send(sd, message, strlen(input)-1, 0);
                }
            }else{
                handle_send_message(input, &more, to_username, to_message, &message_length, &message_header, &sd);
            }
        // IF the the socket is ready to be read
        }else if (FD_ISSET(sd, &ready_to_read_set)){
            // socket     
            int n = recv(sd, &control_header, 2, 0);
            if (n == 0){
                PRINT_MSG("Lost connection with the server\n");
                exit(EXIT_SUCCESS);
            }
            control_header = ntohs(control_header);
            // if control header
            if((control_header & 0x8000) == 0){
                handle_control_header(&control_header, &mt, &code, &unc, &max_ulen,  &input_max,
                                &sd, message, &username_accepted, client_username);
            }else{
                // this is a message header
                // recv the other 16 bytes from the server
                recv(sd, &message_header, 2, 0);
                // print_u16_as_bits(control_header);
                message_header = ntohl(message_header);
                // print_u32_as_bits(message_header);
                temp_message_header += control_header << 16;
                // bit shift the just recv'd bytes and append the previously recv'd 16 bytes
                message_header = message_header >> 16;
                message_header += temp_message_header;     
                // print_u32_as_bits(message_header);
                parse_chat_header(&message_header, &mt, &pub, &prv, &frg, &lst, &ulen, &len);
                // printf("MT: %u, pub: %u, prv: %u, frg: %u, lst: %u, ulen: %u, len: %u\n", mt, pub, prv, frg, lst, ulen, len);
                // if the message is private
                if (prv){
                    recv(sd, from_username, ulen, 0);
                    recv(sd, from_message, len, 0);
                    PRINT_PRIVATE_MSG(from_username, client_username, from_message);
                }else if(pub){
                    recv(sd, from_username, ulen, 0);
                    recv(sd, from_message, len, 0);
                    PRINT_PUBLIC_MSG(from_username, from_message);
                }
            }
        }       
    }
}

/**
 * @brief This function handles sending a message to the server
 * 
 * @param input 
 * @param more 
 * @param to_username 
 * @param to_message 
 * @param message_length 
 * @param message_header 
 * @param sd 
 */
void handle_send_message(char* input, int *more, char* to_username, char* to_message, int *message_length, uint32_t *message_header, int *sd){
    read_stdin(input, 5000, more);
    if(input[0] == '\n'){
        return;
    }
    // removes the \n from the input
    input[strlen(input) - 1] = '\0';
    // printf("input is %s\n", input);
    // if the first char of the input is @
    if ((int)input[0] == 64){
        //private message
        // find the first space and get the username and message
        for(int i = 0; i < strlen(input); i++){
            if ((int)input[i] == 32){
                input[i] = '\0';
                to_username = input;
                to_message = input + strlen(to_username) + 1;
                *message_length = strlen(to_message);
                break;
            }
        }
        if (!to_message || !*message_length){
            return;
        }
        // remove the @ from the username
        to_username++;
        if ((strlen(to_message) / 255) == 0){
            // if the message is less than 255, send the chat 
            create_chat_header(message_header, 1, 0, 1, 0, 0, strlen(to_username), strlen(to_message));
            *message_header = htonl(*message_header);
            send(*sd, message_header, 4, 0);
            send(*sd, to_username, strlen(to_username), 0);
            send(*sd, to_message, strlen(to_message), 0);
        }else{
            for (int i = 0; i < floor(*message_length / 255) + 1; i++){
                // if this is the last fragment
                if (floor(strlen(to_message) / 255) == 0){
                    // create the last header and send it
                    create_chat_header(message_header, 1, 0, 1, 1, 1, strlen(to_username), strlen(to_message));
                    *message_header = htonl(*message_header);
                    send(*sd, message_header, sizeof(*message_header), 0);
                    send(*sd, to_username, strlen(to_username), 0);
                    send(*sd, to_message, strlen(to_message), 0);
                }else{
                    // not the last fragment
                    create_chat_header(message_header, 1, 0, 1, 1, 0, strlen(to_username), 255);
                    *message_header = htonl(*message_header);
                    send(*sd, message_header, sizeof(*message_header), 0);
                    send(*sd, to_username, strlen(to_username), 0);
                    send(*sd, to_message, 255, 0);
                    // update to_username to not include previously sent message
                    to_message += 255;
                }
            }
        }            
    }else{
        to_message = input;
        *message_length = strlen(to_message);
        // public message
        if ((strlen(to_message) / 255) == 0){
            // if the message is less than 255, send the chat 
            create_chat_header(message_header, 1, 1, 0, 0, 0, 0, strlen(to_message));
            *message_header = htonl(*message_header);
            send(*sd, message_header, 4, 0);
            // send(sd, to_username, strlen(to_username), 0);
            send(*sd, to_message, strlen(to_message), 0);
        }else{
            for (int i = 0; i < floor(*message_length / 255) + 1; i++){
                // if this is the last fragment
                if (floor(strlen(to_message) / 255) == 0){
                    // create the last header and send it
                    create_chat_header(message_header, 1, 1, 0, 1, 1, 0, strlen(to_message));
                    *message_header = htonl(*message_header);
                    send(*sd, message_header, sizeof(*message_header), 0);
                    send(*sd, to_message, strlen(to_message), 0);
                }else{
                    // not the last fragment
                    create_chat_header(message_header, 1, 1, 0, 1, 0, 0, 255);
                    *message_header = htonl(*message_header);
                    send(*sd, message_header, sizeof(*message_header), 0);
                    send(*sd, to_message, 255, 0);
                    // update to_username to not include previously sent message
                    to_message += 255;
                }
            }
        }
    }
}

/**
 * @brief This function handles receiving the control header and informing the client 
 * 
 * @param control_header 
 * @param mt 
 * @param code 
 * @param unc 
 * @param max_ulen 
 * @param input_max 
 * @param sd 
 * @param message 
 * @param username_accepted 
 * @param client_username 
 */
void handle_control_header(uint16_t *control_header, uint8_t *mt, uint8_t *code, uint8_t *unc, uint8_t *max_ulen, int* input_max,
int* sd, char* message, int* username_accepted, char * client_username){
// the header is the first two bytes
    // network to host
    // control_header = ntohs(control_header);
    parse_control_header(control_header, mt, code, unc, max_ulen);
    if (*code == 0){
        PRINT_MSG("Server is full\n");
        exit(EXIT_SUCCESS);
    }
    // if the code is 1, the server is not full
    else if(*code == 1){
        *input_max = *max_ulen;
        // ask user for username
        PRINT_wARG("Choose a username (should be less than %d): ", *max_ulen + 1);
    }
    // if the code is 2, display a player has joined
    else if(*code == 2){
        recv(*sd, message, *max_ulen, 0);
        PRINT_USER_JOINED(message);
    }
    // if the code is 3, display the player has left
    else if(*code == 3){
        recv(*sd, message, *max_ulen, 0);
        PRINT_USER_LEFT(message);
    }
    // if the code is 4, handle message negotiations 
    else if(*code == 4){
        if (*unc == 0){
            PRINT_MSG("Time to enter username has expired. Try again.\n");
            exit(EXIT_FAILURE);
        }else if (*unc == 1){
            PRINT_MSG("Username is taken. Try again.\n");
            PRINT_wARG("Choose a username (should be less than %d): ", *input_max + 1);
        }else if (*unc == 2){
            PRINT_MSG("Username is too long. Try again.\n");
            PRINT_wARG("Choose a username (should be less than %d): ", *input_max + 1);
        }else if (*unc == 3){
            PRINT_MSG("Username is invalid. Only alphanumerics are allowed. Try again.\n");
            PRINT_wARG("Choose a username (should be less than %d): ", *input_max + 1);
        }else if(*unc == 4){
            recv(*sd, message, *max_ulen, 0);
            strcpy(client_username, message);
            PRINT_MSG("Username accepted\n");
            *username_accepted = 1;                   
        }
    }
    // if the code is 5, the message was too long
    else if(*code == 5){
        PRINT_MSG("% Couldn't send message. It was too long.\n");
    }
    // if the code is 6, the username specified was invalid
    else if(*code == 6){
        recv(*sd, message, *max_ulen, 0);
        PRINT_INVALID_RECIPIENT(message);
    }
    // if the code is 7, private message is incorrect
    else if(*code == 7){
        PRINT_MSG("% Warning: Incorrectly formatted private message. Missing recipient.\n");
    }
}