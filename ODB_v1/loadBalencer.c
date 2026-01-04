#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h> // read(), write(), close()
#include "websocket.h"
#define MAX 4096
#define PORT_CLIENT 8080
#define PORT_WEB_SERVER 8081
#define SA struct sockaddr
static int c = 0;


char *tcp_verify(char *buff)
{
    char method[8], path[64], protocol[16];
    if (sscanf(buff, "%7s %63s %15s", method, path, protocol) == 3)
    {
        if ((strcmp(method, "GET") == 0 || strcmp(method, "POST") == 0) &&
            strncmp(protocol, "HTTP/", 5) == 0)
        {
            printf("\n requet est methode : %7s path : %63s protocole: %15s\n", method, path, protocol);
            return strdup(path); // allocation dynamique
        }
    }
    return NULL;
}
char* get_type(char *path){
    char *ext = strrchr(path, '.'); 
        if (ext!=NULL){
            ext++;
        }
    if(!ext) return "application/octet-stream";

    // text / application
    if (strcmp(ext, "html") == 0) return "text/html";
    if (strcmp(ext, "htm")  == 0) return "text/html";
    if (strcmp(ext, "css")  == 0) return "text/css";
    if (strcmp(ext, "js")   == 0) return "application/javascript";
    if (strcmp(ext, "txt")  == 0) return "text/plain";
    if (strcmp(ext, "xml")  == 0) return "application/xml";
    if (strcmp(ext, "json") == 0) return "application/json";
    
    // image
    if (strcmp(ext, "png")  == 0) return "image/png";
    if (strcmp(ext, "jpg")  == 0 || strcmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, "gif")  == 0) return "image/gif";
    if (strcmp(ext, "svg")  == 0) return "image/svg";

    // audio
    if (strcmp(ext, "mp3")  == 0) return "audio/mpeg";
    if (strcmp(ext, "wav")  == 0) return "audio/wav";
    if (strcmp(ext, "flac") == 0) return "audio/flac";

    // video
    if (strcmp(ext, "mp4")  == 0) return "video/mp4";
    if (strcmp(ext, "webm") == 0) return "video/webm";
    if (strcmp(ext, "avi")  == 0) return "video/x-msvideo";

    // document
    if (strcmp(ext, "pdf")  == 0) return "application/pdf";
    if (strcmp(ext, "zip")  == 0) return "application/zip";
    if (strcmp(ext, "gz")   == 0) return "application/gzip";
    if (strcmp(ext, "tar")  == 0) return "application/x-tar";
    return "application/octet-stream";
}
char *header_analyse(char* buffer, char *path,size_t n) {
    size_t content_length = 0;
    char *header = malloc(MAX);
    if (!header) {
        perror("malloc");
        exit(1);
    }

    char head_length[MAX];
    int hlen = 0;
    char *slice = malloc(n + 1); // +1 pour le '\0'
    if (!slice) {
        perror("malloc");
        return 1;
    }

    memcpy(slice, buffer, n); // copier __n premiers octets
    slice[n] = '\0'; 
    char *p = strstr(slice, "Content-Length:");
    if (!p) {
        free(header);
        return buffer;  
    }

    if (sscanf(p, "Content-Length: %zu", &content_length) == 1 && content_length >= 0) {
        snprintf(head_length, MAX, "Content-Length: %zu\r\n", content_length);
        printf("Header-TCP is %s\n",buffer);
        char ETAT[MAX];
        if (content_length>0){
            snprintf(ETAT,sizeof(ETAT),"200 OK");
        }else{
            snprintf(ETAT,sizeof(ETAT),"400 BAD");
        }
        char *body = buffer + strlen(head_length);
        if (strlen(body) > 0) {
            printf("HA with  \n");
            hlen = snprintf(
                header, MAX,
                "HTTP/1.1 %s \r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %zu\r\n"
                "Connection: close\r\n\r\n"
                "%s",ETAT,
                get_type(path),
                content_length,body
            );
        } else {
            printf("HA without\n");
            hlen = snprintf(
                header, MAX,
                "HTTP/1.1 %s\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %zu\r\n"
                "Connection: close\r\n\r\n",
                ETAT,get_type(path),
                content_length
            );
        }
        printf("Header-TCP-END is %s\n",header);
        return header;
    }

    free(header);
    return buffer;
}


void send_client(int sock_balence_webSever,int connfd_client,char* path,char* buffer){
    bzero(buffer, MAX);
    size_t n = read(sock_balence_webSever, buffer, MAX - 1);
    size_t content_length = 0;
    size_t length=0;

    char *header = malloc(MAX);
    if (!header) {
        perror("malloc");
        exit(1);
    }

    char head_length[MAX];
    int hlen = 0;
    char *slice = malloc(n + 1);
    if (!slice) {
        perror("malloc");
        return 1;
    }

    memcpy(slice, buffer, n); 
    slice[n] = '\0'; 
    char *p = strstr(slice, "Content-Length:");
    if (!p) {
        free(header);
        exit(1);
    }

    if (sscanf(p, "Content-Length: %zu", &content_length) == 1 && content_length >= 0) {
        snprintf(head_length, MAX, "Content-Length: %zu\r\n", content_length);
        char ETAT[MAX];
        if (content_length>0){
            snprintf(ETAT,sizeof(ETAT),"200 OK");
        }else{
            snprintf(ETAT,sizeof(ETAT),"400 BAD");
        }
        char *body = buffer + strlen(head_length);
        if (strlen(body) > 0) {
            length=(size_t)strlen(head_length);
            hlen = snprintf(
                header, MAX,
                "HTTP/1.1 %s \r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %zu\r\n"
                "Connection: close\r\n\r\n"
                "%s",ETAT,
                get_type(path),
                content_length,body
            );
        } else {
            hlen = snprintf(
                header, MAX,
                "HTTP/1.1 %s\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %zu\r\n"
                "Connection: close\r\n\r\n",
                ETAT,get_type(path),
                content_length
            );
        }
        buffer=header;
    }
    if (n<strlen(buffer)){
            n=strlen(buffer);
    }
    write(connfd_client, buffer,n);
    while(length<content_length){
        n = read(sock_balence_webSever, buffer, MAX - 1);
        if (n <= 0){
            break;
        }
        if (n<strlen(buffer)){
            n=strlen(buffer);
        }
        write(connfd_client, buffer,n);
        length+=n;
        printf("FE length :%zu\n",length);
    }
}


void send_Client_Balencer(int connfd_client, int sock_balence_webSever)
{
    char buff[MAX];
    int n;
    // bzero(buff, MAX);
    //  read the message from client and copy it in buffer
    bzero(buff, MAX);
    n = read(connfd_client, buff, sizeof(buff));
    if (n <= 0)
    {
        printf("client déconnecté\n");
        return;
    }
    // print buffer which contains the client contents
    c += 1;
    printf("\n=*=*=*=*=\n TCP [%d] reçue est : %s\n=*=*=*=*=\n ", c, buff);
    char *path = malloc(64 * sizeof(char));
    if (path == NULL)
        exit(1);
    path = tcp_verify(buff);
    if (path != NULL)
    {
        // char* response=malloc(MAX);
        // if (!response) exit(1);
        char *buff = malloc(10 * MAX);
        if (!buff)
        {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        //printf("path %s est de type %s\n",path,get_type(path));
        write(sock_balence_webSever, path, strlen(path));
        //printf("write balencer to webserver %s\n", path);
        
        int d=0;
        for (;;){
            bzero(buff, MAX);
            n = read(sock_balence_webSever, buff, MAX - 1);
            buff=header_analyse(buff,path,n);
            if (n <= 0)
            {
                break;
            }
                d+=1;
            if (n<strlen(buff)){
                n=strlen(buff);
            }
            //printf("FE recoie %d eme %zu\n",d,n);
            write(connfd_client, buff,n);
        }
    
      
   
    //send_client(sock_balence_webSever,connfd_client,path,buff);

    
    }
    else
    {
        const char *body = "<h1>404 Bad Request</h1>";
        char response[10 * MAX];
        int hlen = snprintf(response, sizeof(response),
                            "HTTP/1.1 400  Request is bad because of structure Methode or path don't existe or ..\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: %ld\r\n"
                            "Connection: close\r\n\r\n"
                            "%s",
                            (long int)strlen(body), body);
        write(connfd_client, response, strlen(response));
    }
}

// Driver send_Client_Balencer
int main()
{
    websocket socket_balence_client = socket_connect(NULL,PORT_CLIENT,0);
    c = 0;
    // len_b_c = sizeof(cli);
    for (;;)
    {
        printf("==================balencer attend un client==================\n");
        accept_client(socket_balence_client);
        websocket sock_fe_is=socket_connect(NULL,PORT_WEB_SERVER,1);
        connection(sock_fe_is);

        // send_Client_Balencertion for chatting between client and balencer and sever
        send_Client_Balencer(socket_balence_client->connfd, sock_fe_is->socket_in);

        // After chatting close the socket
        close(socket_balence_client->connfd);
        printf("\n=*=*=*=*=\nTCP[%d] TERMINE\n=*=*=*=*=\n", c);
    }
}