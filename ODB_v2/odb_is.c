
//netstat -tulnp | grep 9000
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
#include <pthread.h>

#define MAXIMUM 4096
#define BUFFER_SIZE 100*4096
#define LT 20

typedef struct sockaddr SA;
static ssize_t (*real_read)(int, void*, size_t) = NULL;
static int (*real_connect)(int, const struct sockaddr *, socklen_t) = NULL;
static ssize_t (*real_write)(int, const void *, size_t) = NULL;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond  = PTHREAD_COND_INITIALIZER;
int ready = 0;
int ct = 0;
pthread_t t;

static int BE_O_fd = 0;
static int slp=1;
static const char *IP = "127.0.1.3";
static int PORT = 8001;
static char *BUFFER = NULL;
static char *FILE_BUFFER = NULL;
static char * LISTE_Adress[LT];
static int index_adress=0;
static long offset_is=0;
static char *saved_buf = NULL;     // Buffer original
static size_t saved_size = 0;      // Taille du buffer en nbr de pagesize



typedef struct segment
{
    char* IP;
    int port;
    long offset;
    long length;  
    
}Segment;




int connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    if (!real_connect) {
        real_connect = dlsym(RTLD_NEXT, "connect");
        if (!real_connect) {
            fprintf(stderr, "dlsym connect failed: %s\n", dlerror());
            _exit(1);
        }
    }
    BE_O_fd=fd;
    return real_connect(fd, addr, addrlen);
}






/*--------------segment_describe-----------------*/
Segment* segment_describe(const char *filedesc,long lg)
{
    printf("EST IL file descripteur  %s ??\n",filedesc);

    

    char size_str[4];
    char ip[16];
    char port_str[6];
    char offset_str[21];
    char length_str[21];

    int offset = 0;

    if (sscanf(filedesc, "%3[^-]-%n", size_str, &offset) != 1) {
        printf("segment_describe Format invalide\n");
        return NULL;
    }

    int nb_blocs = atoi(size_str);
    if (nb_blocs <= 0 ) {
        printf("Nombre de blocs invalide : %d\n", nb_blocs);
        return NULL;
    }
    printf("Nombre de blocs détectés : %d\n", nb_blocs);
    const char *ptr = filedesc + offset;
    
    
    for (int i = 0; i < nb_blocs; i++) {
        int consumed = 0;

        if (sscanf(ptr,"%15[^-]-%5[^-]-%20[^-]-%20[^-]-%n",ip, port_str, offset_str, length_str, &consumed) != 4)
        {
            printf("Erreur parsing bloc %d\n", i);
            return NULL;
        }
        if (lg<atoi(length_str)){
            Segment* a=calloc(1,sizeof(*a));
            if(!a) exit(2);
            a->IP=strdup(ip);
            a->port=atoi(port_str);
            a->offset=atoi(offset_str)+lg;
            a->length=4096;
            return a;
        }
        lg-=atoi(length_str);
        ptr += consumed;
    }
    return NULL;
}





/* ---------- Envoi du fichier au client ---------- */

ssize_t send_all(int fd, long offset, long length_max) {
    size_t sent = 0;
    while (sent < (size_t)length_max) {
        ssize_t n = (*real_write)(fd, BUFFER+offset + sent, (size_t)length_max - sent);
        if (n <= 0) return n;
        sent += n;
    }
    return sent;
}


/* ---------- Accept client ---------- */
void accepter_client(int sock) {
    int connfd = accept(sock, NULL, NULL);
    if (connfd < 0) { perror("accept"); exit(3); }
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

/* ---------- Thread  IS ---------- */
void *thread_is(void *arg) {
    (void)arg;
    pthread_mutex_lock(&mutex);
    BUFFER = malloc(BUFFER_SIZE);
    if (!BUFFER) { perror("malloc"); exit(4); }
    FILE_BUFFER = malloc(3*MAXIMUM);
    if (!FILE_BUFFER) { perror("malloc"); exit(5); }
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
    if (bind(sock, (SA*)&addr, sizeof(addr)) != 0) { perror("bind"); exit(6); }
    if (listen(sock, 5) != 0) { perror("listen"); exit(7); }
    printf("[DEBUG] Backend écoute %s:%d\n", IP, PORT);

    while (1) accepter_client(sock);
}


/*---------------------socket pour connection-----------------------------*/
websocket socket_connection(const char *ip, int port){
    int sock_IS_be;
    struct sockaddr_in IS_be_addr;

    // socket create and verification 
    sock_IS_be = socket(AF_INET, SOCK_STREAM, 0); 
    if (sock_IS_be == -1) { 
        printf("socket IS et client creation failed...\n"); 
        exit(8); 
    } 
    else
        printf("socket IS et client successfully created..\n"); 
    bzero(&IS_be_addr, sizeof(IS_be_addr)); 
  
    // assign IP, PORT_CLIENT 
    IS_be_addr.sin_family = AF_INET; 
    IS_be_addr.sin_port = htons(port); 

    if (inet_pton(AF_INET, ip, &IS_be_addr.sin_addr) <= 0) {
        fprintf(stderr, "[connexion_backend] Adresse IP invalide : %s\n", ip);
        close(sock_IS_be);
        exit(9);
    }
    int opt = 1;
    setsockopt(sock_IS_be, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (real_connect(sock_IS_be, (SA*)&IS_be_addr, sizeof(IS_be_addr))!= 0) {
        if(slp){
                slp=0;
                printf("[ODB] SLEEP est attendre l'ecoute à connection \n");
                sleep(2);
                return socket_connection(ip,port);
            }
            printf("connection with backend  failed...\n");
            exit(10);
    }
    else  printf("connected to backend..\n");
    websocket socket=malloc(sizeof(socket));
    if (socket==NULL){
        exit(11);
    }
    slp=1;
    socket->socket_addr=IS_be_addr;
    socket->connfd=-1;
    socket->socket_in=sock_IS_be;
    return socket;
}



void printf_segment(Segment* a){
    printf("IP %s port %d offset %ld length %ld\n",a->IP,a->port,(long)a->offset,(long)a->length);
}


/*********CONNECT TO OLD IS OR BE********* */
websocket connexion_is_client(const char *ip, int port)
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



void segv_handler(int sig, siginfo_t *si, void *unused) {
    printf("[ODB] HANDLER\n");

    size_t pagesize = getpagesize();
    void *addr = si->si_addr;
    void *page = (void *)((uintptr_t)addr & ~(pagesize - 1));

    uintptr_t start = (uintptr_t)saved_buf;
    uintptr_t end   = start + saved_size;
    uintptr_t p     = (uintptr_t)page;



    if (p < start || p >= end) {
        printf(
            "p %p start %p end %p ==> difference %ld\n",
            (void *)p,
            (void *)start,
            (void *)end,
            (long)((char *)end - (char *)start)
        );
        signal(SIGSEGV, SIG_DFL);
        raise(SIGSEGV);
        return;
    }

    
    
    printf("adres %p => page adress  %p/ buf adress %p\n",addr,page,(void *)saved_buf);
    if (mprotect(page, pagesize, PROT_READ | PROT_WRITE) != 0) {
        printf("failed mprotect\n");
        exit(12);
    }
    
    long point=(long)((char*)page - (char*)saved_buf);

    Segment* a=segment_describe(FILE_BUFFER,point);
    if(a==NULL) {
        printf("failed segment\n");
    }
    else{
        websocket socket_ws=connexion_is_client(a->IP,a->port);
        char buffer_fd[500] = {0};
        snprintf(buffer_fd,500,"%ld-%zu",a->offset,a->length);
        int length_recue=(size_t)0;
        int length_max=a->length;
        
        int count=MAXIMUM;
        int n=real_write(socket_ws->socket_in,buffer_fd,500);
        while(length_recue<length_max){
            size_t to_read=(size_t)length_max-length_recue;
            if(to_read>count) to_read=count;
            size_t r = real_read(socket_ws->socket_in, page+length_recue, to_read);
            if (r > 0) {
                length_recue += r;
            }
            else {
                printf("failed send\n");
                exit(14);
            }
        }
        LISTE_Adress[index_adress++] = page;
    }
    
}

void new_file_descripteur(long real_offset,char* file,int len){
    char size[5];             
    char ip[16];
    char port_str[6];
    char offset_str[21];
    char length_str[21];

    int c = 0;

    int m = sscanf(file, "%4[^-]-%n",size, &c);



    int i=0;
    char *resp = file + c ;
    int d=c;
    int blocs_number=atoi(size);
    int new_size=blocs_number;

    while (i<blocs_number){
        m = sscanf(resp,"%15[^-]-%5[^-]-%20[^-]-%20[^-]-%n",ip, port_str, offset_str, length_str, &c);
        printf("resp = %s et off %ld\n", resp,real_offset);

        if (real_offset<atoi(length_str)){
            break;
        }
        real_offset-=(long)atoi(length_str);
        resp+=c;
        i+=1;
    }
    char *response = malloc(800);
    if (!response) {
        exit(15);
    }

    int remaining = 800;
    int length_max=0;;
    if (real_offset != 0) {
        int written = snprintf(response+length_max, remaining,"%s-%d-%ld-%ld-",ip,atoi(port_str),atol(offset_str),real_offset);
        length_max+=written;
        remaining  -= written;
        new_size++;
    }

    int written = snprintf(response+length_max, remaining,"%s-%d-%ld-%ld-",IP,PORT,offset_is,(long)getpagesize());
    remaining  -= written;
    length_max+=written;


    if (atoi(length_str) - (getpagesize() + real_offset) > 0) {
        written = snprintf(response+length_max, remaining,"%s-%d-%ld-%ld-",ip,atoi(port_str),atol(offset_str) + real_offset + 4096,atoi(length_str) - (4096 + real_offset));
        remaining  -= written;
        length_max+=written;
        new_size++;

    }
    printf("remember %s\n", response);

    char *filedescp = malloc(len);
    if (!filedescp) {
        exit(16);
    }




    int j = 0;
    written = 0;
    int tl = 0;                    
    remaining = len;         
    written = snprintf(filedescp + tl, remaining, "%d-", new_size);           
    tl += written;
    remaining -= written;
    
    
    resp = file + d;
    while (j < blocs_number) {     // <= remplace size_t par ta vraie variable
        m = sscanf(resp, "%15[^-]-%5[^-]-%20[^-]-%20[^-]-%n",
                ip, port_str, offset_str, length_str, &c);

        if (m < 4) break;          
        if (j == i) {
            written = snprintf(filedescp + tl, remaining, "%s", response);
        } else {
            written = snprintf(filedescp + tl, remaining,
                            "%s-%d-%ld-%ld-",
                            ip,
                            atoi(port_str),
                            atol(offset_str),
                            atol(length_str));
        }

        if (written < 0 || written >= remaining) {
            fprintf(stderr, "Buffer overflow detected\n");
            break;
        }

        tl += written;
        remaining -= written;
        resp += c;
        j++;
    }
    if (tl > 0) {
        filedescp[tl - 1] = '\0';
    }
    bzero(file,len);
    snprintf(file,len,"%s",filedescp);
    free(filedescp);
    free(response);
}




char *FILE_FACT(char* file_name,long len){
    
    
    char size[4];
    char ip[16];
    char port_str[6];
    char offset_str[21];
    char length_str[21];

    int c = 0;
    if (sscanf(file_name, "%3[^-]-%n", size, &c) != 1) {
        printf("FILE_FACT  Format invalide\n");
        return NULL;
    }

    int nb_blocs = atoi(size);
    if (nb_blocs <= 0 ) {
        printf("Nombre de blocs invalide : %d\n", nb_blocs);
        return NULL;
    }    
    char *resp = file_name + c ;

    Segment *segm_lis = malloc(nb_blocs * sizeof(Segment));

    int ind=0;
    
    int m=0;
    m = sscanf(resp,"%15[^-]-%5[^-]-%20[^-]-%20[^-]-%n",ip, port_str, offset_str, length_str, &c);
    printf("[%d] IP %s PORT %d OFFSET %ld LENGTH %ld\n",ind,ip,atoi(port_str),(long)atoi(offset_str),(long)atoi(length_str));
    segm_lis[ind].IP = strdup(ip);
    segm_lis[ind].port=atoi(port_str);
    segm_lis[ind].offset=atoi(offset_str);
    segm_lis[ind].length=atoi(length_str);
    ind+=1;
    resp+=c;
    
    
    for(int i=1;i<nb_blocs;i++){
        m = sscanf(resp,"%15[^-]-%5[^-]-%20[^-]-%20[^-]-%n",ip, port_str, offset_str, length_str, &c);
        if (atoi(port_str)==segm_lis[ind-1].port){
            segm_lis[ind-1].length+=atoi(length_str);
        }
        else{
            printf("[%d] IP %s PORT %d OFFSET %ld LENGTH %ld\n",ind,ip,atoi(port_str),(long)atoi(offset_str),(long)atoi(length_str));
            segm_lis[ind].IP = strdup(ip);
            segm_lis[ind].port=atoi(port_str);
            segm_lis[ind].offset=atoi(offset_str);
            segm_lis[ind].length=atoi(length_str);
            ind+=1;
        }
        resp+=c;
    }
    int remaining = len;   
    char *response = malloc(remaining);
    if (!response) {
        exit(17);
    }

    int written=0;
    int offset_response=0;
    written = snprintf(response+offset_response, remaining-offset_response,"%d-",ind);
    offset_response+=written;


    for(int j=0;j<ind;j++){
        printf_segment(&segm_lis[j]);
        written = snprintf(response+offset_response, remaining-offset_response,"%s-%d-%ld-%ld-",segm_lis[j].IP,segm_lis[j].port,segm_lis[j].offset,segm_lis[j].length);
        offset_response+=written;

    }
    response[offset_response-1]='\0';
    printf("response est %s\n",response);


    return response;
}














void adresse_increase(char** LISTE,int len){
    for(int i=0;i<len-1;i++){
        for (int j=i+1;j<len;j++){
            if ((long)((char*)LISTE[j] - (char*)LISTE[i])<0){
                char* b=LISTE[i];
                LISTE[i]=LISTE[j];
                LISTE[j]=b;
            }
        }
    }
}
void buffer_load(){
    adresse_increase(LISTE_Adress,index_adress);
    
    for(int i=0;i<index_adress;i++){
        printf("[%d/%d] => %p\n",i,index_adress,(void *)LISTE_Adress[i]);
    }
    offset_is=0;
    bzero(BUFFER, BUFFER_SIZE);
    for (int j=0;j<index_adress;j++){
        printf("[%d]\n",j);
        memcpy(BUFFER+offset_is,LISTE_Adress[j],(size_t)getpagesize());
        new_file_descripteur((long)((char*)LISTE_Adress[j] - (char*)saved_buf),FILE_BUFFER,3*MAXIMUM);
        printf("new _file descripteur est %s\n",FILE_BUFFER);
        offset_is+=(long)getpagesize();
    }
    char*response=FILE_FACT(FILE_BUFFER,3*MAXIMUM);
    bzero(FILE_BUFFER,3*MAXIMUM);
    memcpy(FILE_BUFFER,response,3*MAXIMUM);
    printf("Finale BFILE descripteur est %s\n",FILE_BUFFER);
}



void free_liste_adress(){
    for(int i=0;i<index_adress;i++){
        LISTE_Adress[i]=NULL;
    }
    index_adress=0;
}





ssize_t write(int __fd, const void *__buf, size_t __n) {
    printf("[ODB] WRITE\n");

    if (!real_write) {
        real_write = dlsym(RTLD_NEXT, "write");
        printf("[DEBUG] Chargement du write original: %p\n", real_write);
        if (!real_write) {
            fprintf(stderr, "Erreur: impossible de trouver write: %s\n", dlerror());
            exit(18);
        }
    }
    long pagesize = sysconf(_SC_PAGESIZE);
    int m = mprotect((void *)((uintptr_t)__buf & ~(pagesize - 1)), saved_size, PROT_READ | PROT_WRITE);
    int n;
    if(__fd!=BE_O_fd){
        if(index_adress>0){
            printf("IS -> NEXT IS OR FE\n");
            buffer_load();
            n=real_write(__fd,FILE_BUFFER,3*MAXIMUM);
            bzero(FILE_BUFFER,3*MAXIMUM);
            free_liste_adress();
        }else n=real_write(__fd, __buf, __n);
    }else n=real_write(__fd, __buf, __n);

    return n;

}







ssize_t read(int fd, void *buf, size_t count)
{
    printf("[ODB] READ\n");
    if (!real_read) {
        real_read = dlsym(RTLD_NEXT, "read");
        if (!real_read) {
            fprintf(stderr, "dlsym read failed: %s\n", dlerror());
            _exit(19);
        }
    }

    if (!real_write) {
        real_write = dlsym(RTLD_NEXT, "write");
        if (!real_write) {
            fprintf(stderr, "Erreur: impossible de trouver write: %s\n", dlerror());
            _exit(20);
        }
    }

    pthread_mutex_lock(&mutex);
    if (!t)
        pthread_create(&t, NULL, thread_is, NULL);
    while (!ready)
        pthread_cond_wait(&cond, &mutex);

    ssize_t n = 0;
    if (fd == BE_O_fd) {
        n = real_read(fd, buf, count);
        printf("buf BE-> IS %s\n",buf);
        if (n > 0) {
            bzero(FILE_BUFFER,3*MAXIMUM);
            memcpy(FILE_BUFFER, buf, 3*MAXIMUM);


            size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
            saved_buf  = buf;
            saved_size = (size_t)(count / pagesize) * pagesize;
            void* saved_page = (void *)((uintptr_t)buf & ~(pagesize - 1));


            printf("count %zu saved_size %zu , start %p end %p\n",count,saved_size,saved_page,(void *) saved_page+saved_size);
            struct sigaction sa = {0};
            sa.sa_flags = SA_SIGINFO;
            sa.sa_sigaction = segv_handler;
            sigemptyset(&sa.sa_mask);
            sigaction(SIGSEGV, &sa, NULL);


            if (mprotect(saved_page, saved_size, PROT_READ) != 0) {
                perror("mprotect");
                _exit(21);
            }
        }
        pthread_mutex_unlock(&mutex);
        return n;
    }
    pthread_mutex_unlock(&mutex);
    return real_read(fd, buf, count);
}
