
//netstat -tulnp | grep 9000
#define _GNU_SOURCE
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "websocket.h"
#include <unistd.h>
#define NBR_MAX 200
typedef struct sockaddr SA;
typedef struct segment
{
    char* IP;
    int port;
    long offset;
    long length;  
    
}Segment;


static Segment* LIST_SEGMENT[NBR_MAX];
static ssize_t (*real_read)(int, void*, size_t) = NULL;
static int (*real_connect)(int, const struct sockaddr *, socklen_t) = NULL;
static websocket socket_ws;
static int fd_FE_IS = -1;
static int index_fe=-1;
static int nombre_blocs=-1;
static size_t length_recue=-1;
static long length_max=0;

static int old_index=-1;






void free_list_segment(void)
{
    for (int i = 0; i < nombre_blocs; i++) {
        if (LIST_SEGMENT[i]) {
            free(LIST_SEGMENT[i]->IP);
            free(LIST_SEGMENT[i]);
            LIST_SEGMENT[i] = NULL;
        }
    }
    nombre_blocs = 0;
    index_fe = 0;
}





void is_file_descripteur(const char *filedesc)
{
    printf("EST IL file descripteur  %s ??\n",filedesc);
    // 🔴 IMPORTANT : libérer l'ancien état
    free_list_segment();


    char size_str[4];
    char ip[16];
    char port_str[6];
    char offset_str[21];
    char length_str[21];

    int offset = 0;

    if (sscanf(filedesc, "%3[^-]-%n", size_str, &offset) != 1) {
        index_fe=-1;
        printf("❌ Format invalide\n");
        return;
    }

    int nb_blocs = atoi(size_str);
    if (nb_blocs <= 0 || nb_blocs > NBR_MAX) {
        index_fe=-1;
        printf("❌ Nombre de blocs invalide : %d\n", nb_blocs);
        return;
    }
    free_list_segment();   
    nombre_blocs = nb_blocs;
    printf("✅ Nombre de blocs détectés : %d\n", nb_blocs);
    const char *ptr = filedesc + offset;
    
    for (int i = 0; i < nb_blocs; i++) {
        int consumed = 0;

        if (sscanf(ptr,"%15[^-]-%5[^-]-%20[^-]-%20[^-]-%n",ip, port_str, offset_str, length_str, &consumed) != 4)
        {
            index_fe=-1;
            printf("❌ Erreur parsing bloc %d\n", i);
            free_list_segment();   
            return;
        }

        LIST_SEGMENT[i] = malloc(sizeof(Segment));
        if (!LIST_SEGMENT[i]) {
            index_fe=-1;
            perror("malloc");
            free_list_segment();
            exit(EXIT_FAILURE);
        }

        LIST_SEGMENT[i]->IP = strdup(ip);
        LIST_SEGMENT[i]->port = atoi(port_str);
        LIST_SEGMENT[i]->offset = atol(offset_str);
        LIST_SEGMENT[i]->length = atol(length_str);

        ptr += consumed;
    }

    printf("✅ File descriptor valide\n");
}



websocket connexion_frontend_client(const char *ip, int port)
{
    int sock_be;
    struct sockaddr_in addr;

    sock_be = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_be < 0) {
        perror("socket");
        return NULL;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock_be);
        return NULL;
    }

    if (real_connect(sock_be, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock_be);
        return NULL;
    }

    websocket ws = malloc(sizeof(*ws));
    if (!ws) {
        perror("malloc");
        close(sock_be);
        return NULL;
    }

    ws->socket_in   = sock_be;
    ws->connfd      = -1;
    ws->socket_addr = addr;

    return ws;
}






int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
    if (!real_connect) {
        real_connect = dlsym(RTLD_NEXT, "connect");
        if (!real_connect) {
            fprintf(stderr, "dlsym connect failed: %s\n", dlerror());
            _exit(1);
        }
    }

    fd_FE_IS = fd;
    return real_connect(fd, addr, len);
}


void print_all_segment(){
    printf("\n=======================================================\n");
    for(int i=0;i<nombre_blocs;i++){
        printf("[%d] IP %s PORT %d OFFSET %ld LENGTH %ld\n",i,LIST_SEGMENT[i]->IP,LIST_SEGMENT[i]->port,LIST_SEGMENT[i]->offset,LIST_SEGMENT[i]->length);
    }
}





ssize_t read(int fd, void *buf, size_t count) {
    if (!real_read) {
        real_read = dlsym(RTLD_NEXT, "read");
        if (!real_read) {
            fprintf(stderr, "dlsym read failed: %s\n", dlerror());
            _exit(1);
        }
    }
    ssize_t r ;
    if (fd==fd_FE_IS){
        if(index_fe==-1){
            r = real_read(fd, buf, count);
            is_file_descripteur(buf);
            if(index_fe==0){
                print_all_segment();
            }
        }
        if(index_fe!=-1){
            if(old_index!=index_fe){
                old_index=index_fe;
                printf("connection to ip %s et port %d\n",LIST_SEGMENT[index_fe]->IP,LIST_SEGMENT[index_fe]->port);
                socket_ws=connexion_frontend_client(LIST_SEGMENT[index_fe]->IP,LIST_SEGMENT[index_fe]->port);
                char buffer_fd[500] = {0};
                snprintf(buffer_fd,500,"%ld-%zu",LIST_SEGMENT[index_fe]->offset,LIST_SEGMENT[index_fe]->length);
                length_recue=(size_t)0;
                length_max=LIST_SEGMENT[index_fe]->length;
                printf("FE->SERVEUR %s\n",buffer_fd);
                int n=write(socket_ws->socket_in,buffer_fd,500);
            }
            if(length_recue<length_max){
                size_t to_read=(size_t)length_max-length_recue;
                if(to_read>count) to_read=count;
                r = real_read(socket_ws->socket_in, buf, to_read);
                if (r > 0) {
                    length_recue += r;
                }
                else if (r == 0) {
                    index_fe=-1;
                    old_index=-1;
                    length_recue=-1;
                    length_max=-1;
                    printf("[INFO] backend fermé la connexion\n");
                    return 0;
                }
                else {
                    index_fe=-1;
                    old_index=-1;
                    length_recue=-1;
                    length_max=-1;
                    perror("read");
                    return -1;
                }
                printf("RECOIE [ind=%d] %zu / %ld\n",index_fe,length_recue,length_max);
                if((long) length_recue>=length_max){
                    index_fe++;
                    if(index_fe==nombre_blocs){
                        printf("envoyé tout \n");
                        index_fe=-1;
                        old_index=-1;
                        length_recue=-1;
                        length_max=-1;
                    }
                }
                return r;
            }

        }
        return r;
    }
    return real_read(fd, buf, count);

}
