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
#define PORT 9000
#define SA struct sockaddr
#define PATH "file"

void send_balencer_webserver(int connfd_webServer)
{
    char buff[MAX];
    int n;
    bzero(buff, sizeof(buff));
    read(connfd_webServer, buff, sizeof(buff));
    const char* path =buff;
    
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s%s",PATH, path);
    printf("full path est %s \n",fullpath);
    bzero(buff, MAX); 
    FILE *fp = fopen(fullpath, "rb");
    if (fp!=NULL) {

        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        rewind(fp);

        char header[256];
        int hlen = snprintf(header, sizeof(header),
            "Content-Length: %ld\r\n",
            size
        );
        write(connfd_webServer, header, hlen);

        // envoyer fichier binaire
        char buffer[MAX];
        size_t n;
        int d=0;
        while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
            write(connfd_webServer, buffer, n);
            d+=1;
            printf("chink envoie %d eme with length %zu\n",d,n);
        }

        fclose(fp);





    }
    else{
        printf("fichier n'existe pas\n");
        char response[10*MAX];
        int hlen = snprintf(response, sizeof(response),
            "Content-Length: %ld \r\n"
            ,
            0);
        printf("header est %s\n",response);
        write(connfd_webServer, response, strlen(response)); 
    }
    
}


int main()
{
    websocket socket=socket_connect(NULL,PORT,0);
    while(1){
        printf("==================Backend attend un Webserve==================r\n");
        accept_client(socket);
        send_balencer_webserver(socket->connfd);
        close(socket->connfd);
    }
}