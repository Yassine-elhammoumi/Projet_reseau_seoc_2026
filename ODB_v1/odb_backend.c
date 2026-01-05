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

typedef struct sockaddr SA;
#define BUFFER_SIZE (5 * 1024 * 1024)

static char *BUFFER = NULL;
static size_t content_length = 0;
static size_t length = 0;
static const char *IP = "127.0.1.1";
static int PORT = 8091;
static long offset = 0;
static int ct=0;
static ssize_t (*original_write)(int, const void *, size_t) = NULL;


/**
 * @brief Structure représentant l’identifiant d’un buffer stocké.
 *
 * Cette structure contient deux informations essentielles :
 *  - offset : position dans le buffer principal où commence le fichier.
 *  - lenght : taille totale du fichier (Content-Length).
 *
 * Elle permet d’identifier précisément où un fichier complet a été
 * stocké dans BUFFER, afin qu'il puisse être renvoyé plus tard.
 */
typedef struct buffer_id {
    long offset;   /// Position de départ du fichier dans BUFFER
    int lenght;    /// Taille totale du contenu associé
} *buffer_id;

static buffer_id buf_id;  /// Identifiant du fichier en cours


/**
 * @brief Initialise une socket serveur pour accepter une connexion avec ODB FRONTEND.
 *
 * Cette fonction :
 *  - crée un socket TCP (AF_INET, SOCK_STREAM)
 *  - configure l’adresse (IP+PORT)
 *  - active SO_REUSEADDR (réutilisation du port)
 *  - effectue bind() puis listen()
 *  - alloue dynamiquement une structure `websocket`
 *  - stocke dans cette structure :
 *      - le socket serveur
 *      - un FD client non encore connecté
 *      - l’adresse du serveur
 *
 * @return websocket  Structure contenant les informations du socket serveur
 *                    (retourne NULL / -1 en cas d’erreur interne)
 */
websocket connection_socket(){
        int sock_FE, connfd_FE;
        struct sockaddr_in balence_FE;
                
        sock_FE = socket(AF_INET, SOCK_STREAM, 0);
        printf("[DEBUG] socket() -> %d\n", sock_FE);
        if (sock_FE == -1) {
            perror("socket");
            return -1;
        }
        memset(&balence_FE, 0, sizeof(balence_FE));
        balence_FE.sin_family = AF_INET;
        balence_FE.sin_port = htons(PORT);
        inet_pton(AF_INET, IP, &balence_FE.sin_addr.s_addr);
        int opt = 1;    
        setsockopt(sock_FE, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (bind(sock_FE, (SA*)&balence_FE, sizeof(balence_FE)) != 0) {
            perror("bind");
            close(sock_FE);
            return -1;
        }
        printf("[DEBUG] bind OK port=%d\n", PORT);
        if (listen(sock_FE, 5) != 0) {
            perror("listen");
            close(sock_FE);
            return -1;
        }
        printf("[DEBUG] listen OK\n");
            // Allocation dynamique de la structure websocket
        websocket ws = malloc(sizeof(*ws));
        if (!ws) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        // Initialisation des champs
        ws->socket_in = sock_FE;      // Socket principal
        ws->connfd    = -1;         // FD de connexion client (non connecté)
        ws->socket_addr = balence_FE;     // Adresse du serveur ou du client

        return ws;
}

/**
 * @brief Accepte un client sur le socket serveur créé par connection_socket().
 *
 * Cette fonction :
 *  - appelle accept() sur ws->socket_in
 *  - vérifie que la connexion est valide
 *  - stocke le FD client dans ws->connfd pour les futures communications
 *
 * @param ws  Structure websocket contenant le socket serveur
 */
void accepter_client(websocket ws){
    int connfd_FE = accept(ws->socket_in, NULL, NULL);
    printf("[DEBUG] BE accept()  FE -> %d\n", connfd_FE);
    if (connfd_FE < 0) {
        perror("accept");
        close(ws->socket_in);
        return -1;
    }
    ws->connfd=connfd_FE;
}

/**
 * @brief Envoie un fichier (body HTTP) stocké dans BUFFER à un client FE.
 *
 * Cette fonction :
 *  - envoie un header indiquant Content-Length
 *  - envoie ensuite successivement les données à partir de
 *      BUFFER[offset_ + sent]
 *  - vérifie à chaque boucle que l’envoi est correct
 *  - protège contre dépassements si la mémoire disponible n'est pas suffisante
 *
 * @param offset_          Position du début du fichier dans BUFFER
 * @param content_length_  Taille totale du contenu à envoyer
 * @param connfd_FE        Descripteur du client à qui envoyer les données
 */
void send_file_client(int offset_,size_t content_length_,int connfd_FE){
        // Construction du header Content-Length à envoyer au client
        char response[256];
                        
        snprintf(response, sizeof(response),"Content-Length: %zu\r\n", content_length_);
                        
        // Envoi du header HTTP
        (*original_write)(connfd_FE, response, strlen(response));
        printf("=*=\n%s\n=*=:\n avec %zu\n",response,strlen(response));                

        // Initialisation du compteur de bytes envoyés
        size_t sent = 0;

        // Boucle d’envoi du body depuis BUFFER
        while (sent < content_length_) {
        
            // Vérification mémoire disponible dans BUFFER
            size_t dispo = BUFFER_SIZE - (offset_ + sent);
        
            if (dispo == 0) {
                printf("[ERREUR] Pas assez de données dans BUFFER !\n");
                break;
            }
        
            // Envoi d’un chunk du fichier
            ssize_t n = (*original_write)(
                connfd_FE,
                BUFFER + offset_ + sent,
                (size_t)content_length_ - sent
            );
        
            // Vérification écriture
            if (n <= 0) {
                printf("[ERREUR] write a échoué (n=%zd)\n", n);
                break;
            }
        
            // Mise à jour du nombre total envoyé
            sent += n;
            printf("[DEBUG] Envoi chunk : %zd bytes (%zu/%zu)\n",
                   n, sent, content_length_);
        }
}




/**
 * Intercepte et étend le comportement de write() pour gérer la logique ODB.
 *
 * Cette fonction remplace temporairement write() grâce au mécanisme LD_PRELOAD.
 * Elle permet d’intercepter les données envoyées par un processus, d’identifier
 * les headers HTTP, de stocker un fichier dans un buffer mémoire, puis de le
 * transmettre via un protocole interne entre ODB_Backend et ODB_Frontend.
 *
 * Fonctionnement général :
 * -----------------------
 * - Résolution dynamique de la fonction write() originale via dlsym(RTLD_NEXT).
 * - Allocation d’un buffer global (BUFFER) une seule fois, au premier appel.
 * - Copie temporaire des __n octets reçus pour analyser uniquement la portion utile.
 *
 * Détection du header HTTP :
 * ---------------------------
 * - Recherche du champ "Content-Length:" dans la tranche reçue.
 * - Si détecté :
 *      * extraction de la taille totale du fichier (content_length),
 *      * création d’une structure buf_id contenant :
 *            - offset : adresse de début de stockage dans BUFFER,
 *            - lenght : taille totale du fichier,
 *      * réinitialisation de length (quantité déjà reçue),
 *      * vérification que la taille du fichier est compatible avec BUFFER,
 *      * retour immédiat car seules les métadonnées (header) ont été reçues.
 *
 * Stockage progressif du fichier :
 * --------------------------------
 * - Les données reçues sont concaténées dans BUFFER à partir de offset + length.
 * - length est incrémenté jusqu’à atteindre content_length.
 * - Tant que length < content_length, write() retourne simplement __n.
 *
 * Réception complète du fichier :
 * -------------------------------
 * - Lorsque length == content_length :
 *      * mise à jour de offset pour préparer une prochaine écriture,
 *      * création d’un fork() :
 *
 *      1. Processus parent :
 *          - fabrique un descripteur logique sous la forme :
 *                IP-port-offset-length
 *          - envoie cet identifiant au serveur intermédiaire (IS).
 *
 *      2. Processus enfant :
 *          - se connecte à ODB_Frontend,
 *          - lit le descripteur renvoyé par celui-ci,
 *          - si le descripteur est valide (m == 4),
 *                → envoie le fichier via send_file_client().
 *          - ferme proprement les sockets utilisés.
 *
 * Retour :
 * --------
 * - Si la fonction n’intercepte rien de particulier, elle appelle simplement
 *   la version originale de write().
 *
 * Utilité de is_IS :
 * ------------------
 * - is_IS sert à identifier si le FD appartient au serveur intermédiaire (IS).
 *   Lorsque is_IS = 1, certaines opérations (lecture bloquante ou envoi)
 *   doivent être adaptées pour éviter les blocages et reconnaître les données
 *   provenant du webServer intermédiaire.
 */

ssize_t write(int __fd, const void *__buf, size_t __n) {

    // On pointe sur la fonction write() originale via dlsym.
    // original_write est static : il ne sera chargé qu'une seule fois,
    // ce qui permet de réutiliser l'adresse de la vraie fonction write()
    // à chaque appel, sans chercher à nouveau dans la table des symboles.
    if (!original_write) {
        original_write = dlsym(RTLD_NEXT, "write");
        printf("[DEBUG] Chargement du write original: %p\n", original_write);
        if (!original_write) {
            fprintf(stderr, "Erreur: impossible de trouver write: %s\n", dlerror());
            exit(1);
        }
    }


    // Allocation du buffer principal utilisé pour stocker les données du fichier.
    // Ne doit être effectuée qu'une seule fois au tout premier appel (ct == 0).
    if (ct == 0) {
        BUFFER = malloc(BUFFER_SIZE);
        printf("[DEBUG] Allocation BUFFER %p (size=%ld)\n", BUFFER, BUFFER_SIZE);
        if (!BUFFER) {
            perror("Erreur d'allocation");
            return -1;
        }
        ct++;  // Indique que l'allocation est déjà faite.
    }

    // On copie exactement __n octets depuis __buf dans une tranche temporaire.
    // Cela évite d'analyser plus que nécessaire (comme un slicing Python).
    // La tranche sert uniquement pour détecter un éventuel header HTTP.
    char *slice = malloc(__n + 1);  // +1 pour ajouter un '\0' et traiter en chaîne C.
    if (!slice) {
        perror("malloc");
        return 1;
    }
    memcpy(slice, __buf, __n);  // Copie des __n premiers octets du buffer source.
    slice[__n] = '\0';          // Null-termination pour strstr/sscanf.


    // Détection d'un header HTTP contenant "Content-Length:"
    // → Cela indique que ce write() correspond à l'envoi d'un header et non du fichier.
    char *p = strstr(slice, "Content-Length:");
    if (p!=NULL && sscanf(p, "Content-Length: %zu", &content_length) == 1 && content_length >= 0) {
        if(content_length==0){
            return (*original_write)(__fd, __buf, __n);
        }
        // Création d'une structure identifiant l'objet reçu (offset + length max).
        // Ce header agit comme un "identifiant" du futur fichier.

        buf_id=malloc(sizeof(*buf_id));
        if(!buf_id){
            perror("Erreur d'allocation");
            return -1;
        }

        printf("[DEBUG][header detecter] Content-Length détecté = %d avec Lecture headers (%zu bytes)\n",
               (int) content_length,__n);

        // Vérification : la taille annoncée doit tenir dans le BUFFER
        if ((long) content_length > BUFFER_SIZE) {
            free(buf_id);
            perror("erreur de taille");
            exit(1);
        }

        // Si l'espace restant dans le buffer est insuffisant → réinitialisation.
        if (offset > BUFFER_SIZE -  (long) content_length ) {
            printf("[ODB ] Reset offset car dépassement (offset=%ld)\n", offset);
            offset = 0;
        }

        // Initialisation du suivi d'écriture pour ce fichier.
        length = 0;                         // Quantité réellement reçue.
        buf_id->lenght=(int)content_length; // Taille totale attendue.
        buf_id->offset=offset;              // Position de départ dans BUFFER.

        p=NULL;
        free(slice);
        return __n;  // On retourne directement : seuls les headers ont été reçus.
    }

    // Nettoyage de la mémoire temporaire si aucun header n'est détecté.
    if(slice) free(slice);


    // Phase de mémorisation des données dans BUFFER.
    // On stocke __n octets à la suite des données déjà reçues (offset + length).
    if (BUFFER && __n > 0 && BUFFER + offset +length+ __n < BUFFER+BUFFER_SIZE) {        
        memcpy(BUFFER + offset + length, __buf, __n);  // Remplissage
        length += __n;                                 // Mise à jour quantité reçue

        printf("[DEBUG] Copie dans BUFFER: length=%d / content_length=%d (received=%d)\n",
               (int)length,(int) content_length,(int) __n);

        // Si le fichier n'est pas encore complètement reçu → rien d'autre à faire.
        if (length < content_length) {
            return __n;
        }
    }
    

    // Si length == content_length → nous avons reçu **100% du fichier**.
    // Le backend peut maintenant notifier l'Intermédiaire Serveur (IS)
    // et lancer un processus enfant pour gérer l'envoi réel du fichier.
    if (BUFFER && length == content_length) {

        // Mise à jour de l'offset pour réserver la zone utilisée.
        offset += content_length;
        printf("[DEBUG] Réception complète (%zu bytes) avec new offset %ld\n",
               content_length,offset);


        pid_t pi=fork();  // Création d'un processus parent + enfant
        char response[256];
        if (pi < 0) {
            perror("fork"); 
        }

        // --------------------------
        //  Processus PARENT (pi > 0)
        // --------------------------
        // Le parent envoie uniquement une courte réponse contenant :
        // IP - PORT - OFFSET - LENGTH
        // Ce "file descriptor logique" sera utilisé par IS pour demander le fichier.
        if (pi>0) {
            printf("[DEBUG] processus principal: envoi réponse courte\n");
            snprintf(response, sizeof(response), "%s-%d-%ld-%d",
                     IP, PORT, buf_id->offset, buf_id->lenght);

            return (*original_write)(__fd, response, strlen(response));
        }

        // --------------------------
        //  Processus ENFANT (pi == 0)
        // --------------------------
        // Le rôle du processus enfant :
        // 1) se connecter à ODB_Frontend
        // 2) recevoir un "file descriptor logique"
        // 3) envoyer les données du fichier via send_file_client()
        else{
            /*
            Étapes du processus enfant :
            - Connexion socket avec ODB FRONTEND
            - Attente d'un message contenant le descripteur logique
            - Extraction des paramètres offset/taille/IP/port
            - Envoi réel du fichier stocké dans BUFFER
            */
                    
            int sock_FE, connfd_FE;
            struct sockaddr_in balence_FE;

            // Création socket + écoute (fonction abstraite interne)
            websocket ws=connection_socket();
            sock_FE=ws->socket_in;
            accepter_client(ws);
            connfd_FE=ws->connfd;

            // Réception du file descriptor logique envoyé par Frontend
            char buffer_fd[100] = {0};
            ssize_t r = read(connfd_FE, buffer_fd, sizeof(buffer_fd) - 1);
            if (r <= 0) {
                close(connfd_FE);
                close(sock_FE);
                return -1;
            }
            buffer_fd[r] = '\0';

            // Extraction des informations : ip - port - offset - length
            char ip_[16];
            int port_, offset_;
            size_t content_length_;

            int m = sscanf(buffer_fd,"%15[^-]-%d-%d-%zu",
                           ip_, &port_, &offset_, &content_length_);

            // Si les informations sont bien reçues → on envoie le fichier demandé
            if (m == 4) {
                send_file_client(offset_,content_length_,connfd_FE);
            }

            // Fermeture propre
            close(connfd_FE);
            close(sock_FE);
            printf("[DECONNEXION ODB]\n");

            if(buf_id) free(buf_id);
            return __n;
        }
    }


    // Si aucun traitement particulier, on appelle simplement le write() original.
    return (*original_write)(__fd, __buf, __n);
}
