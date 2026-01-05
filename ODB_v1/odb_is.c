
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


typedef struct sockaddr SA;


#define MAX 4096
static  char ip[16], port_str[6], offset_str[21], length_str[21]; 
static int backend_fd = -1;    // socket connecté au backend
static websocket ws_s = NULL;  // structure websocket si nécessaire
//static void * addr_aligned;//delete
//static void * adress;//delete


static char *saved_buf = NULL;     // Buffer original
static size_t saved_size = 0;      // Taille du file descripteur qui est dans buffer
static size_t saved_all= 0;
static void *saved_page = NULL;    // Adresse alignée sur la page
static char* buffer;//buffer pour avoir good mapping

static int lenght=0;
static int pt=0;
static int BE_O_fd = 0;
static int first=1;
static int appel_handler=0;
static int slp=1;


static ssize_t (*real_read)(int, void*, size_t) = NULL;
//static ssize_t (*real_write)(int, void*, size_t) = NULL;
static int (*real_connect)(int, const struct sockaddr *, socklen_t) = NULL;

/**
 * @brief Crée et initialise un socket TCP pour se connecter au backend ODB.
 *
 * Cette fonction :
 *  - crée un socket TCP IPv4,
 *  - initialise une structure `sockaddr_in` avec l'adresse IP et le port du backend,
 *  - configure l'option SO_REUSEADDR,
 *  - établit directement la connexion avec `connect()`, avec retry si nécessaire,
 *  - alloue dynamiquement une structure `websocket` pour stocker :
 *        • l'adresse du backend,
 *        • le socket TCP utilisé,
 *        • l'état de connexion.
 *
 * @param ip   Adresse IP du backend (ex : "127.0.0.1").
 * @param port Port TCP du backend (ex : 9000).
 *
 * @return Un pointeur vers la structure `websocket` initialisée.
 *         Le programme termine avec exit() en cas d’erreur.
 */



websocket connexion_backend(const char *ip, int port){
    int sock_IS_be;
    struct sockaddr_in IS_be_addr;

    // socket create and verification 
    sock_IS_be = socket(AF_INET, SOCK_STREAM, 0); 
    if (sock_IS_be == -1) { 
        printf("socket IS et client creation failed...\n"); 
        exit(0); 
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
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    setsockopt(sock_IS_be, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (real_connect(sock_IS_be, (SA*)&IS_be_addr, sizeof(IS_be_addr))!= 0) {
        if(slp){
                slp=0;
                printf("[ODB] SLEEP est attendre l'ecoute à connection \n");
                sleep(2);
                return connexion_backend(ip,port);
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
    socket->socket_addr=IS_be_addr;
    socket->connfd=-1;
    socket->socket_in=sock_IS_be;
    ws_s=socket;
    return socket;
}

/**
 * @brief Établit la connexion vers le backend ODB et transmet la requête initiale.
 *
 * Fonctionnement :
 *  - Analyse les variables globales `ip`, `port_str`, `offset_str`, `length_str`.
 *  - Convertit port et longueur en entier/long.
 *  - Crée une connexion via `connexion_backend()`.
 *  - Envoie la requête initiale au backend avec `write()`.
 *  - Met à jour `backend_fd` avec le socket connecté.
 *
 * Variables globales utilisées :
 *  - ip, port_str, offset_str, length_str : infos pour la connexion backend.
 *  - ws_s : websocket associé au backend.
 *  - backend_fd : fd du socket connecté.
 *  - saved_size : taille du buffer initial.
 */

void backend_odb_conn(){
    first=0;
    pt=0;
    int port = atoi(port_str);
    long offset = atol(offset_str);
    long length_ = atol(length_str);
    lenght=length_;
    char response[256];
    fprintf(stderr, "[hook read] IP=%s PORT=%d OFFSET=%ld LENGTH=%ld\n", ip, port, offset, length_);
    ws_s = connexion_backend((char *) ip, (int) port);  // doit renvoyer websocket avec socket_in valide
    snprintf(response, sizeof(response), "%s-%d-%ld-%ld",ip, port, offset, length_);
    if (!ws_s) {
        fprintf(stderr, "connexion_ODB-BACKEND failed\n");
        exit(1);
    }
    backend_fd = ws_s->socket_in;
    printf("[ODB CONNECXION] WIRTE[%ld] to ODB BACKEND %s \n",strlen(response),response);
    ssize_t w = write(backend_fd, response, strlen(response));
    if (w != saved_size) perror("forwarding initial control to backend failed");    
}


/**
 * @brief Hook de la fonction connect() pour tracer et mémoriser les connexions.
 *
 * Cette fonction intercepte les appels à connect() via LD_PRELOAD :
 *  - Initialise `real_connect` via dlsym() si nécessaire.
 *  - Si `backend != 1` : mémorise le fd dans `BE_O_fd` pour suivi.
 *  - Si `backend == 1` : affiche que la connexion est vers le ODB backend.
 *  - Appelle ensuite la vraie fonction connect() pour établir la connexion.
 *
 * @param fd      File descriptor du socket.
 * @param addr    Adresse de connexion.
 * @param addrlen Taille de l’adresse.
 *
 * @return Résultat de la vraie fonction connect().
 */

int connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    if (!real_connect) {
        real_connect = dlsym(RTLD_NEXT, "connect");
        if (!real_connect) {
            fprintf(stderr, "dlsym connect failed: %s\n", dlerror());
            _exit(1);
        }
    }

    BE_O_fd=fd;


    
    //printf("frontend creer une connexion avec webServer avec id %d\n",BE_O_fd);
    return real_connect(fd, addr, addrlen);
}

/**
 * @brief Gestionnaire de signal SIGSEGV pour protéger le buffer et reconnecter.
 *
 * Fonctionnement :
 *  - Capture un crash de segmentation (SIGSEGV).
 *  - Rétablit les droits de lecture/écriture sur la page contenant `saved_buf`.
 *  - Réinitialise le buffer avec memset.
 *  - Appelle `backend_odb_conn()` pour rétablir la connexion au backend.
 *  - Réinitialise `first` pour signaler qu’on attend une nouvelle lecture.
 *
 * @param sig Numéro du signal reçu (SIGSEGV).
 */

// Handler SIGSEGV
void segv_handler(int sig) {
    printf("\n=== SIGSEGV capturé ===\n");
    //write(STDERR_FILENO, msg, sizeof(msg)-1);
    printf("buffer changer %s\n",saved_buf);
    mprotect(saved_page, saved_all, PROT_READ | PROT_WRITE);
    memset(saved_buf, 0, saved_all);
    backend_odb_conn();
    first=0;
    appel_handler=1;
}

/**
 * @brief Hook de la fonction read() pour intercepter les données du backend ODB.
 *
 * Fonctionnement :
 *  - Initialise `real_read` via dlsym() si nécessaire.
 *  - Si le fd correspond à `BE_O_fd` et `first == 1` :
 *       • lit le buffer initial envoyé par le serveur,
 *       • extrait IP, port, offset et longueur via sscanf,
 *       • protège la page mémoire du buffer avec mprotect(PROT_READ),
 *       • installe `segv_handler` pour gérer les crashs,
 *       • retourne le nombre d'octets lus.
 *  - Si fd correspond à `BE_O_fd` et `first == 0` :
 *       • lit les données depuis le backend ODB jusqu’à ce que `pt >= lenght`,
 *       • met à jour `pt`, déconnecte et réinitialise le backend.
 *  - Si fd n’est pas `BE_O_fd`, appelle simplement la vraie fonction read().
 *
 * Variables globales :
 *  - saved_buf, saved_page, saved_size : pour la protection mémoire.
 *  - ws_s : websocket vers le backend.
 *  - pt, lenght, first : suivi de lecture et état de la connexion.
 *  - backend_fd : fd du socket backend.
 *
 * @param fd    File descriptor à lire.
 * @param buf   Buffer où stocker les données.
 * @param count Taille maximale à lire.
 *
 * @return Nombre d'octets lus ou -1 en cas d'erreur.
 */


ssize_t read(int fd, void *buf, size_t count) {
    if (!real_read) {
        real_read = dlsym(RTLD_NEXT, "read");
        if (!real_read) {
            fprintf(stderr, "dlsym read failed: %s\n", dlerror());
            _exit(1);
        }
    }
    size_t n=0;
    if(fd==BE_O_fd){
        if(first){

            n = real_read(fd, buf, count);

            if (n > 0) {
            
                printf("buffer est %s\n",(char * )buf);
                // Extraction des champs
                sscanf(buf, "%15[^-]-%5[^-]-%20[^-]-%20s", ip, port_str, offset_str, length_str);
                saved_buf = buf;
                saved_size = strlen(buf);

                struct sigaction sa = {0};
                sa.sa_handler = segv_handler;
                sigaction(SIGSEGV, &sa, NULL);
                
                size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
                saved_all=(size_t)(count / pagesize) * pagesize;
                // Adresse de la page contenant le buffer
                saved_page = (void *)((uintptr_t)buf & ~(pagesize - 1));

                printf("before mprotect buf %p saved_page %p\n",buf,saved_page);

                // Protection de *la page du buffer*
                int m = mprotect(saved_page, saved_all , PROT_READ);

                printf("etat de mprotect est %d\n", m);

                if (m != 0) {
                    perror("mprotect");
                    exit(1);
                }
            }

            return n;
        }
        else{
            if (pt < lenght) {
                printf("READ from ODB-BACKEND lenght %d pt %d\n",lenght,pt);
                n=real_read(ws_s->socket_in, buf, count);
                pt+=n;
                printf("pt est %d\n",pt);
                printf("[ODB] FE  [LENGHT LUE=%d]/[Content-Lenght %d]\n",pt,lenght);
                if (pt>=lenght && ws_s !=NULL){
                    pt=0;// add in moment
                    lenght=0;// add in moment
                    printf("[ODB] Deconnection de ODB-BACKEND\n");
                    close(ws_s->socket_in);
                    first=1;
                }
                return n;
            }
        }
    }
    
    return real_read(fd, buf, count);
}

/**
 * @brief Hook de la fonction write() pour protéger la mémoire avant écriture.
 *
 * Fonctionnement :
 *  - Charge la vraie fonction write() via dlsym() si nécessaire.
 *  - Calcule la page mémoire du buffer à écrire et active PROT_READ | PROT_WRITE.
 *  - Appelle la vraie fonction write() pour envoyer les données.
 *
 * @param __fd  File descriptor pour écrire.
 * @param __buf Buffer contenant les données à écrire.
 * @param __n   Nombre d’octets à écrire.
 *
 * @return Nombre d’octets effectivement écrits ou -1 en cas d’erreur.
 */


ssize_t write(int __fd, const void *__buf, size_t __n) {

    static ssize_t (*original_write)(int, const void *, size_t) = NULL;
    if (!original_write) {
        original_write = dlsym(RTLD_NEXT, "write");
        printf("[DEBUG] Chargement du write original: %p\n", original_write);
        if (!original_write) {
            fprintf(stderr, "Erreur: impossible de trouver write: %s\n", dlerror());
            exit(1);
        }
    }
    if(appel_handler==1){
        appel_handler=0;
        return __n;
    }

    size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
    int m = mprotect((void *)((uintptr_t)__buf & ~(pagesize - 1)), saved_all, PROT_READ | PROT_WRITE);
    int n=original_write(__fd, __buf, __n);

    return n;
}