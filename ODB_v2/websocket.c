#include "websocket.h"
#define SA struct sockaddr

/**
 * @brief Crée un socket TCP pour un serveur ou un client.
 *
 * Cette fonction initialise un socket, configure l'adresse IP et le port,
 * puis effectue bind + listen si le mode serveur est choisi, ou prépare
 * le socket pour une connexion si le mode client est choisi.
 *
 * @param ip L'adresse IP du serveur à connecter. NULL pour INADDR_ANY (serveur).
 * @param port Le port TCP utilisé pour la connexion.
 * @param is_client Indique le mode :
 *                  - 0 : mode serveur (bind + listen)
 *                  - 1 : mode client (prêt à connecter)
 * @return websocket Structure contenant le socket, l'adresse et le fd de connexion.
 */
websocket socket_connect(char* ip,int port, int is_client) {
    int sock_;
    struct sockaddr_in addr;

    // Création du socket TCP
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    printf("[INFO] Socket créé (port %d)\n", port);

    // Initialisation de la structure sockaddr_in
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    if(ip==NULL) {
        // Si IP NULL → accepter toutes les interfaces (serveur)
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        printf("=====================ip %s\n", ip);
        if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
            fprintf(stderr, "[connexion_backend] Adresse IP invalide : %s\n", ip);
            close(sock_);
            exit(EXIT_FAILURE);
        }
    }
    addr.sin_port = htons(port);

    // Mode serveur : bind + listen
    if (!is_client) {
        if (bind(sock_, (SA*)&addr, sizeof(addr)) != 0) {
            perror("bind");
            exit(EXIT_FAILURE);
        }
        if (listen(sock_, 5) != 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }
        printf("[INFO] Serveur en écoute sur le port %d\n", port);
    }

    // Allocation dynamique de la structure websocket
    websocket ws = malloc(sizeof(*ws));
    if (!ws) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Initialisation des champs
    ws->socket_in = sock_;      // Socket principal
    ws->connfd    = -1;         // FD de connexion client (non connecté)
    ws->socket_addr = addr;     // Adresse du serveur ou du client

    return ws;
}

/**
 * @brief Accepte une connexion entrante pour un serveur.
 *
 * Attends un client qui se connecte au socket principal et met à jour
 * le champ `connfd` de la structure websocket.
 *
 * @param ws Structure websocket contenant le socket serveur.
 */
void accept_client(websocket ws) {
    int connfd = accept(ws->socket_in, NULL, NULL);
    if (connfd < 0) {
        perror("accept");
        return;
    }

    printf("[INFO] Client connecté.\n");
    ws->connfd = connfd; // Met à jour le fd de connexion pour le client
}

/**
 * @brief Établit une connexion à un serveur distant (mode client).
 *
 * Configure le socket pour réutiliser l'adresse locale, puis tente
 * la connexion à l'adresse et port spécifiés dans la structure websocket.
 *
 * @param ws Structure websocket contenant le socket client et l'adresse serveur.
 */
void connection(websocket ws) {
    int opt = 1;
    // Permet de réutiliser l'adresse locale immédiatement
    setsockopt(ws->socket_in, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (connect(ws->socket_in, (SA*)&ws->socket_addr,sizeof(ws->socket_addr)) != 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("[INFO] Connecté au serveur.\n");
}
