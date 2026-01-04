#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include "websocket.h"
#define BUFFER_SIZE (1 * 1024 * 1024)

typedef struct sockaddr SA;

static char *BUFFER = NULL;
static long content_length = 0;
static size_t length = 0;
static const char *IP = "127.0.1.1";
static int PORT = 8011;
static long offset = 0;
static ssize_t (*original_write)(int, const void *, size_t) = NULL;
static FILE *(*original_fopen) (const char *__restrict __filename,const char *__restrict __modes)=NULL;


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond  = PTHREAD_COND_INITIALIZER;
int ready = 0;
int ct = 0;
pthread_t t;



/* ---------- fopen intercepté ---------- */
FILE *fopen(const char *filename, const char *mode) {
    if (!original_fopen)
        original_fopen = dlsym(RTLD_NEXT, "fopen");
    FILE *fp = original_fopen(filename, mode);
    if (fp) {
        fseek(fp, 0, SEEK_END);
        content_length = ftell(fp);
        rewind(fp);
    } else content_length = 0;
    printf("[DEBUG] FILE %s size=%ld\n", filename, (long)content_length);
    return fp;
}

/* ---------- Envoi du fichier au client ---------- */

ssize_t send_all(int fd, long offset, long length_max) {
    size_t sent = 0;
    while (sent < (size_t)length_max) {
        ssize_t n = (*original_write)(fd, BUFFER+offset + sent, (size_t)length_max - sent);
        if (n <= 0) return n;
        sent += n;
    }
    return sent;
}


/* ---------- Accept client ---------- */
void accepter_client(int sock) {
    int connfd = accept(sock, NULL, NULL);
    if (connfd < 0) { perror("accept"); exit(1); }
    printf("[DEBUG] Client connecté: %d\n", connfd);
    char buffer_fd[500] = {0};
    long offset;
    long length_max;
    ssize_t r = read(connfd, buffer_fd, sizeof(buffer_fd) - 1);
    printf("client send to backend %s\n",buffer_fd);
    int m = sscanf(buffer_fd, "%d-%zu", &offset, &length_max);
    if(m==2) send_all(connfd,offset,length_max);
    shutdown(connfd, SHUT_WR);
}

/* ---------- Thread serveur backend ---------- */
void *thread_backend(void *arg) {
    (void)arg;
    pthread_mutex_lock(&mutex);
    BUFFER = malloc(BUFFER_SIZE);
    if (!BUFFER) { perror("malloc"); exit(1); }
    ready = 1;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, IP, &addr.sin_addr);
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(sock, (SA*)&addr, sizeof(addr)) != 0) { perror("bind"); exit(1); }
    if (listen(sock, 5) != 0) { perror("listen"); exit(1); }
    printf("[DEBUG] Backend écoute %s:%d\n", IP, PORT);

    while (1) accepter_client(sock);
}



char* file_descripteur(long content_length) {
    const long SIZE = 4096;
    int blocs = (content_length+SIZE-1) / SIZE;
    int MAX_RES=3*4096;
    char *response = malloc(MAX_RES);
    if (!response) return NULL;

    long offset = 0;
    int total = 0;

    total += snprintf(response + total, MAX_RES - total, "%d-", blocs);

    for (int i = 0; i < blocs; i++) {

        long chunk=SIZE;
        if (offset+SIZE>content_length){
            chunk=content_length-offset;
        }
        total += snprintf(response + total, MAX_RES - total,
                          "%s-%d-%ld-%ld-",
                          IP, PORT, offset, chunk);

        offset += chunk;
    }

    response[total - 1] = '\0'; // enlève le dernier '-'
    return response;
}


ssize_t write(int __fd, const void *__buf, size_t __n) {
    if (!original_write)
        original_write = dlsym(RTLD_NEXT, "write");

    pthread_mutex_lock(&mutex);
    if (!t) pthread_create(&t, NULL, thread_backend, NULL);
    while (!ready) pthread_cond_wait(&cond, &mutex);


    if(content_length==0){
        pthread_mutex_unlock(&mutex);
        return (*original_write)(__fd, __buf, __n);
    }
    if (length==0 && content_length> 0) {
        bzero(BUFFER,BUFFER_SIZE);
        if ((long) content_length > BUFFER_SIZE) {
            pthread_mutex_unlock(&mutex);
            perror("erreur de taille");
            exit(1);
        }
    }

    if (BUFFER && __n > 0 && length+ __n < BUFFER_SIZE) {        
        memcpy(BUFFER + length, __buf, __n);  
        length += __n;                                

        printf("[DEBUG] Copie dans BUFFER: length=%d / content_length=%d (received=%d)\n",(int)length,(int) content_length,(int) __n);

        if (length < content_length) {
            pthread_mutex_unlock(&mutex);
            return __n;
        }
    }


    if (BUFFER && length == content_length) {

        char response[256];
        snprintf(response, sizeof(response), "1-%s-%d-%d-%d",IP, PORT, 0, content_length);
        length = 0; 
        //printf("content length est %ld\n",content_length);
        //char* response=file_descripteur(content_length);
        //length=0;
        pthread_mutex_unlock(&mutex);                    
        return (*original_write)(__fd, response, strlen(response));
    }
    pthread_mutex_unlock(&mutex);
    return (*original_write)(__fd, __buf, __n);
}

