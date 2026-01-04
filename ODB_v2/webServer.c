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

#define MAX 5*4096
#define PORT_balencer 8081
#define PORT_backend 9000

#define SA struct sockaddr
static char* buff;

int add_comment_after_head(char *buf, size_t buf_size)
{
    const char *comment = "\n<!-- Modified by ODB -->\n";
    const char *tag = "<head>";

    char *pos = strstr(buf, tag);
    if (!pos) return -1;

    pos += strlen(tag);

    size_t comment_len = strlen(comment);
    size_t tail_len = strlen(pos);

    if (strlen(buf) + comment_len + 1 > buf_size)
        return -1;

    memmove(pos + comment_len, pos, tail_len + 1);
    memcpy(pos, comment, comment_len);

    return 0;
}


void change(char *buff){
    printf("buff changer %p / %p\n",(void *) buff+500,(void *)buff);
    for(int i=0;i<=100;i++){
        buff[500+i]='A';
    }
    printf("buff changer %p / %p\n",(void *) buff+4096,(void *)buff);
    for(int i=0;i<=100;i++){
        buff[4096+i]='B';
    }
    printf("buff changer %p / %p\n",(void *) buff+(2*4096),(void *)buff);
    for(int i=0;i<=100;i++){
        buff[(4*4096)+i]='C';
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
        printf("le message recue de ! ! ! Backend  est %s\n", buff);
        if (n < 100 && strstr(buff ? buff : "", "404") != NULL)
        {
            printf("file n'existe pas\n");
        }else change(buff);

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
    }

    // close the socket
    //close(sock_balencer_webserver);
}