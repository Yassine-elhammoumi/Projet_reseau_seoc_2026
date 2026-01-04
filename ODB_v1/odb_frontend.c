
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


typedef struct sockaddr SA;


#define MAX 4096

static int backend_fd = -1;    // socket connecté au backend
static websocket ws_s = NULL;  // structure websocket si nécessaire
static int content_length_body=0;
static int length_read_total=0;
static int IS_fd = 0;
static int  is_file_descripteur=1;
static int is_IS=1;
static int slp=1;


static ssize_t (*real_read)(int, void*, size_t) = NULL;
static int (*real_connect)(int, const struct sockaddr *, socklen_t) = NULL;

/**
 * @brief Établit une connexion socket vers un backend (serveur).
 *
 * Cette fonction crée un socket TCP IPv4, initialise une structure
 * `sockaddr_in` avec l'adresse IP et le port fournis, puis alloue
 * dynamiquement une structure `websocket` contenant les informations
 * nécessaires pour communiquer avec le backend.
 *
 * Aucun appel à connect() n'est réalisé ici : la fonction prépare
 * uniquement le socket et les informations associées.
 *
 * @param ip   Adresse IP du ODB_backend sous forme de chaîne (ex : "127.0.0.1").
 * @param port Port du ODB_backend (ex : 9000).
 *
 * @return Un pointeur `websocket` correctement initialisé.
 *         Le programme termine avec exit() en cas d’erreur.
 */
websocket connexion_backend(const char *ip, int port){
    int sock_balence_be;
    struct sockaddr_in balence_be_addr;
  
    // Création du socket TCP (AF_INET + SOCK_STREAM)
    sock_balence_be = socket(AF_INET, SOCK_STREAM, 0); 
    if (sock_balence_be == -1) { 
        printf("socket balence et client creation failed...\n"); 
        exit(0); 
    } 
    else
        printf("socket balence et client successfully created..\n"); 

    // Initialise la structure d'adresse avec des zéros
    bzero(&balence_be_addr, sizeof(balence_be_addr)); 
  
    // Remplissage de l'adresse (famille + port converti big-endian)
    balence_be_addr.sin_family = AF_INET; 
    balence_be_addr.sin_port = htons(port); 

    // Conversion de l’IP en format binaire + vérification validité
    if (inet_pton(AF_INET, ip, &balence_be_addr.sin_addr) <= 0) {
        fprintf(stderr, "[connexion_backend] Adresse IP invalide : %s\n", ip);
        close(sock_balence_be);
        exit(EXIT_FAILURE);
    }

    // Allocation de la structure websocket (espace mémoire dynamique)
    websocket socket=malloc(sizeof(socket));
    if (socket==NULL){
        exit(1);
    }

    // Réinitialise la variable globale slp
    slp=1;

    // Remplit les champs de la structure websocket
    socket->socket_addr=balence_be_addr;   // Adresse du backend
    socket->connfd=-1;                     // Pas de connexion "acceptée"
    socket->socket_in=sock_balence_be;     // Socket utilisé pour envoyer/recevoir

    // Retourne la structure websocket prête à l'utilisation
    return socket;
}



/**
 * @brief Établit la connexion au ODB backend via un socket WebSocket.
 *
 * Cette fonction tente d'établir une connexion TCP avec l'adresse stockée
 * dans `socket->socket_addr`. Avant cela, elle active l'option SO_REUSEADDR
 * pour permettre la réutilisation de l'adresse locale.
 * 
 * - En cas d'échec, si `slp` vaut 1, la fonction attend 2 secondes puis retente.
 * - Si l'échec persiste, le programme se termine.
 *
 * @param socket  Structure contenant le socket et l'adresse du serveur odb backend.
 */
void connection(websocket socket){
    int opt = 1;

    /* Active SO_REUSEADDR pour permettre la réutilisation du port local */
    setsockopt(socket->socket_in, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Tentative de connexion au backend */
    if (connect(socket->socket_in, (SA*)&socket->socket_addr, sizeof(socket->socket_addr)) != 0) {

        /* Si un "retry" est autorisé via slp, attendre 2 secondes et réessayer */
        if (slp) {
           slp = 0; // slp est remis à 0 : autorise une seule tentative de reconnexion
            printf("[ODB] SLEEP : attente avant nouvelle tentative de connection\n");
            sleep(2);

            /* Rappel récursif pour retenter la connexion */
            return connection(socket);
        }

        /* Si la connexion échoue malgré tout */
        printf("connection with backend failed...\n");
        exit(0);
    }
    else {
        /* Connexion réussie */
        printf("connected to backend..\n");
    }
}



/**
 * @brief Hook de la fonction connect() pour intercepter les connexions.
 *
 * Cette fonction remplace temporairement `connect()` via LD_PRELOAD.
 * Elle permet de capturer et tracer les connexions vers le webserver
 * et le backend, puis appelle la vraie fonction connect().
 *
 * @param fd      indice du socket
 * @param addr    Adresse du serveur
 * @param addrlen Taille de l'adresse
 * @return Résultat de la fonction connect réelle
 *
 * @note La variable globale `is_IS` est utilisée pour distinguer les types
 *       de connexion :
 *       - `is_IS == 1` : la connexion concerne le nouveau WebServer,
 *         on mémorise le fd dans `IS_fd`.
 *       - `is_IS != 1` : la connexion concerne le odb backend.
 */
int connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    /* Initialisation du pointeur vers la vraie fonction connect() si nécessaire */
    if (!real_connect) {
        real_connect = dlsym(RTLD_NEXT, "connect");
        if (!real_connect) {
            fprintf(stderr, "dlsym connect failed: %s\n", dlerror());
            _exit(1);
        }
    }

    /* Si c'est le serveur IS, mémoriser le fd et afficher message */
    if (is_IS == 1) {
        printf("CONNECTE TO NEW WEBSERVER avec %d\n", fd);
        IS_fd = fd;
    }
    else {
        /* Sinon, on trace la connexion vers le odb backend */
        printf("[ODB][CONNECT] FOR CONNECTE TO ODB BACKEND avec %d\n", fd);
    }

    /* Appel de la vraie fonction connect() */
    return real_connect(fd, addr, addrlen);
}

/**
 * @brief Hook de la fonction read() pour intercepter les lectures depuis un socket.
 *
 * Cette fonction remplace temporairement `read()` via LD_PRELOAD.
 * Elle permet de capturer les données lues depuis le WebServer et de les
 * rediriger vers le backend si nécessaire.
 *
 * Comportement spécifique :
 * - Si le fd correspond à `IS_fd` (connexion WebServer) :
 *     - Lors de la première lecture (` is_file_descripteur == 1`), la requête initiale reçue du WebServer via une nouvelle connexion.
 *       pour extraire l'IP, le port, l'offset et la longueur du contenu si la requête provient du WebServer (frontend) attendu, auquel cas `m` vaut 4 cad on a reçue bien un file descripteur .  
 *       Dans le cas contraire (`m` différent de 4), cela signifie que le WebServer (IS) a modifié les données, et on recevra les données correctes mais pas le format attendu.
 *     -Donc si m=4(file descripteur)
 *     - Une connexion vers le odb backend est créée via `connexion_backend()`
 *       et `connection()`.
 *     - on memorise la taille complete de donnée qu'on doit recevoir d'apres odb backend 'length_read_total`
 *     - La requête est renvoyée au odb backend .
 *     - on intialise maintenant la taille du donnée que va recevoir `length_read_total`
 *     - On met ` is_file_descripteur` à 0 car ce paramètre indique si l'on attend la requête initiale du WebServer (IS)  qui contient file descripteur.  
 *       Une fois cette requête reçue et contient file descripteur et pas des données modifié par IS, ` is_file_descripteur` passe à 0 pour signaler que les lectures suivantes proviendront du backend ODB, et non plus du WebServer.  
 *       Ainsi, la fonction `read` n'essaiera pas de lire à nouveau depuis le fd du WebServer, qui pourrait être bloqué si aucune donnée n’est envoyée.
 *     - Les réponses du odb backend sont lues et renvoyées au frontend.
 *      - Les variables globales utilisées :
 *     - ` is_file_descripteur` : indique si on attend des requetes (comme file descripteur) du webServeur et pas ODB backend.
 *     - `length_read_total` : nombre total d’octets lus depuis le backend.
 *     - `content_length_body` : longueur totale attendue du contenu à lire.
 *     - `ws_s` : structure websocket du backend.
 *     - `backend_fd` : fd du socket connecté au backend.
 *     - `is_IS` : indique si on est en communication avec le WebServer (1) ou le backend (0).
 *
 * @param fd    indice du socket.
 * @param buf   Buffer où stocker les données lues.
 * @param count Taille maximale à lire.
 *
 * @return Nombre d'octets lus, ou -1 en cas d’erreur.
 */
ssize_t read(int fd, void *buf, size_t count) {
    /* Initialisation du pointeur vers la vraie fonction read() si nécessaire */
    if (!real_read) {
        real_read = dlsym(RTLD_NEXT, "read");
        if (!real_read) {
            fprintf(stderr, "dlsym read failed: %s\n", dlerror());
            _exit(1);
        }
    }

    int m = 0;
    char ip[16], port_str[6], offset_str[21], length_str[21]; 
    size_t n = 0;

    /* Si le fd correspond au WebServer */
    if (fd == IS_fd) {
        printf("READ FROM IS\n");
        /* Lecture initiale et parsing de la requête */
        if ( is_file_descripteur) {
            printf("file descripteur \n");
            n = real_read(fd, buf, count);
            m = sscanf(buf, "%15[^-]-%5[^-]-%20[^-]-%20s", ip, port_str, offset_str, length_str);   
        }

        /* Première lecture valide : établir la connexion au backend */
        if ( is_file_descripteur && m == 4) {
             is_file_descripteur = 0;
            length_read_total = 0;

            printf("La requête prévue est bien arrivée : %s\n", (char *) buf);
            int port = atoi(port_str);
            long offset = atol(offset_str);
            long length_ = atol(length_str);
            content_length_body = length_;

            fprintf(stderr, "[hook read] IP=%s PORT=%d OFFSET=%ld LENGTH=%ld\n",
                    ip, port, offset, length_);

            /* Connexion au backend */
            is_IS = 0;// is_IS revient à 0 pour ne pas memoriser indice du socket odb backend lors de l'appel du fonction `connect()`
            ws_s = connexion_backend(ip, port);  
            connection(ws_s);
            is_IS = 1;// puis on remet à 1 

            if (!ws_s) {
                fprintf(stderr, "connexion_backend failed\n");
                return -1;
            }

            backend_fd = ws_s->socket_in;

            /* Transmission de la requête initiale au backend */
            ssize_t w = write(backend_fd, buf, strlen(buf));
            if (w != n) perror("forwarding initial control to backend failed");  

            /* Nettoyage du buffer et lecture de la réponse */
            bzero(buf, MAX);  
            n = real_read(backend_fd, buf, MAX);
            printf("=*=\n%s\n=*=:\n avec %zu octets\n", buf, n);                
            length_read_total += n;
            return n;
        }

        /* Lectures suivantes jusqu’à épuisement du contenu */
        if (length_read_total < content_length_body) {
            printf("READ from BACKEND,  is_file_descripteur=%d\n",  is_file_descripteur);
            n = real_read(ws_s->socket_in, buf, MAX);
            length_read_total += n;

            printf("[ODB] FE [LENGHT LUE=%d]/[Content-Lenght %d]\n", 
                   length_read_total, content_length_body);

            if (length_read_total >= content_length_body && ws_s != NULL) {
                 is_file_descripteur = 1;
                length_read_total = 0;
                content_length_body = 0;
                printf("[ODB] Déconnexion du backend\n");
                close(ws_s->socket_in);
            }
            return n;
        }
        return n;
    }

    /* Si ce n’est pas le fd du WebServer, lecture normale */
    return real_read(fd, buf, count);
}
