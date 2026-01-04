#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct websocket_s {
    int socket_in;               // Socket principal
    int connfd;                  // Connexion acceptée (en mode serveur)
    struct sockaddr_in socket_addr; // Adresse du socket
} *websocket;

websocket socket_connect(char*  ip,int port, int is_client);
void accept_client(websocket ws);
void connection(websocket ws);

#endif
