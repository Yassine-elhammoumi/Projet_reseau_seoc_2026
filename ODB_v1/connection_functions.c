#define _GNU_SOURCE
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <malloc.h>
#include <stdint.h>
#include "connection_functions.h"


websocket odb_connexion_backend_fact(const char *ip, int port, websocket ws_s, int slp){
    int sock;
    struct sockaddr_in sock_addr;

    // socket create and verification 
    sock = socket(AF_INET, SOCK_STREAM, 0); 
    if (sock == -1) { 
        printf("socket creation failed...\n"); 
        exit(0); 
    } 
    else
        printf("socket successfully created..\n"); 
    bzero(&sock_addr, sizeof(sock_addr)); 
  
    // assign IP, PORT_CLIENT 
    sock_addr.sin_family = AF_INET; 
    sock_addr.sin_port = htons(port); 

    if (inet_pton(AF_INET, ip, &sock_addr.sin_addr) <= 0) {
        fprintf(stderr, "[connexion_backend] Adresse IP invalide : %s\n", ip);
        close(sock);
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (connect(sock, (SA*)&sock_addr, sizeof(sock_addr))!= 0) {
        if(slp){
                slp=0;
                printf("[ODB] SLEEP est attendre l'ecoute à connection \n");
                sleep(2);
                return odb_connexion_backend_fact(ip,port, ws_s, slp);
            }
            printf("connection with backend  failed...\n");
            exit(0);
    }
    else  printf("connected to backend..\n");
    websocket socket=malloc(sizeof(socket));
    if (socket==NULL){
        exit(1);
    }
    slp=1;
    socket->socket_addr=sock_addr;
    socket->connfd=-1;
    socket->socket_in=sock;
    ws_s=socket;
    return socket;
}

int odb_connect_fact(int fd, const struct sockaddr *addr, socklen_t addrlen, int (*real_connect)(int, const struct sockaddr *, socklen_t), int file_descriptor, int backend, int first) {
    if (!real_connect) {
        real_connect = dlsym(RTLD_NEXT, "connect");
        if (!real_connect) {
            fprintf(stderr, "dlsym connect failed: %s\n", dlerror());
            _exit(1);
        }
    }

    if (backend!=1){
            printf("CONNECTE TO NEW WEBSERVER avec %d\n",fd);
            file_descriptor=fd;
    }
    if (backend == 1){
                first=1;
                printf("[ODB][CONNECT] FOR CONNECTE TO BACKEND avec %d\n",fd);
    }

    
    //printf("frontend creer une connexion avec webServer avec id %d\n",IS_fd);
    return real_connect(fd, addr, addrlen);
}

websocket setup_server(int port){
    websocket socket = server_creation(port);
    if (bind(socket->socket_in, (SA*)&socket->socket_addr, sizeof(socket->socket_addr)) != 0) {
        perror("Socket bind failed ");
        exit(EXIT_FAILURE);
    }

    if (listen(socket->socket_in, 5) != 0) {
        perror("Listen failed ");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", port);
    return socket;
}

websocket server_creation(int port){
    websocket sock=malloc(sizeof(sock));;
    // socket create and verification
    sock->socket_in = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->socket_in == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else printf("Socket successfully created..\n");

    bzero(&sock->socket_addr, sizeof(sock->socket_addr));
    // assign IP, PORT
    sock->socket_addr.sin_family = AF_INET;
    sock->socket_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    sock->socket_addr.sin_port = htons(port);
    return sock; 
}

websocket accept_connection(websocket socket){
    int len = sizeof(socket->socket_addr);
    socket->connfd = accept(socket->socket_in, (SA*)&socket->socket_addr, &len);
    if (socket->connfd  < 0) {
        printf("Server don't accepted a connection of FE\n");
        perror("accept");
        exit(1);

    }
    printf("Server accepted a connection of server\n");
    return socket;
}

websocket connect_servers(int port){
    websocket socket = server_creation(port);
    if (connect(socket->socket_in, (SA*)&socket->socket_addr, sizeof(socket->socket_addr))
            != 0) {
            printf("connection failed...\n");
            exit(0);
        }
        else
            printf("connected..\n");
}