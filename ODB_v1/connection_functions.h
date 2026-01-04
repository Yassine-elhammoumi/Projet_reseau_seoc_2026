#ifndef FONCTION_H
#define FONCTION_H

#define _GNU_SOURCE
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "websocket.h"
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <malloc.h>
#include <stdint.h>


typedef struct sockaddr SA;

websocket odb_connexion_backend_fact(const char *ip, int port, websocket ws_s, int slp); 

int odb_connect_fact(int fd, const struct sockaddr *addr, socklen_t addrlen, int (*real_connect)(int, const struct sockaddr *, socklen_t), int file_descriptor, int backend, int first);

websocket setup_server(int port); 

websocket accept_connection(websocket socket);

websocket connect_servers(int port);

websocket server_creation(int port);

#endif
