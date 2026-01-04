#include <arpa/inet.h>   // inet_addr()
#include <netdb.h>
#include <sys/socket.h>

#include <sys/mman.h>    // mmap, munmap, PROT_*, MAP_*
#include <unistd.h>      // sysconf, _SC_PAGESIZE, read(), write(), close()
#include <stdint.h>      // uintptr_t

#include <stdio.h>       // printf, perror
#include <stdlib.h>      // exit, malloc, free
#include <string.h>      // memset, memcpy, strlen, strcpy
#include <strings.h>     // bzero()
#include "websocket.h"

#define MAX 4096
#define PORT_balencer 8081
#define PORT_backend 9000

#define SA struct sockaddr
static char* buff;
static int ct=1;
void change(char *buff){
    if(ct==1){
        buff[0]='a';
        memset(buff, 0, strlen(buff));
        ct=0;
    }
}


int read_write(int socket_reception, int socket_envoi ,int i){

    bzero(buff, MAX);
    int n=read(socket_reception, buff, MAX);
    if (n<=0){
        return 0;
    }
    if (i==0) {
        printf("le message recue de ! ! ! Load Balencer est %s\n", buff);
    }else{
        //printf("le message recue de ! ! ! Backend  est %s\n", buff);
        //char a=buff[0];
        //buff[0]=' ';
        //buff[0]=a;
        //change(buff);

    }
    write(socket_envoi, buff, n); 
    //printf("le message after changer  %.10s\n", buff);
    return 1;
}

void send_balencer_webserver(int connfd_balencer,int sock_webSever_backend){
    read_write(connfd_balencer, sock_webSever_backend,0);
    while(1){
        if (read_write(sock_webSever_backend, connfd_balencer ,1)==0) return;
    }
    
}

int main()
{
    websocket socket=socket_connect(NULL,PORT_balencer,0);

//
    //sock_balencer_webserver = setup_server(balencer_webserver_addr, sock_balencer_webserver);
    //char buff[MAX];
    buff = (char*) mmap(NULL, MAX,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, // <-- indispensable
                          -1, 0);
    if (buff == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    while(1){
        printf("==================WebServer attend un balencer==================\n");
        accept_client(socket);
        websocket sock_webSever_backend =socket_connect(NULL,PORT_backend,1);
        connection(sock_webSever_backend);

        // send_balencer_webservertion for chat
        send_balencer_webserver(socket->connfd,sock_webSever_backend->socket_in);
        //close(sock_webSever_backend);
        close(socket->connfd);
        ct=1;
    }

    // close the socket
    //close(sock_balencer_webserver);
}