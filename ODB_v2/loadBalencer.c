#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "websocket.h"

#define MAX 2*4096
#define PORT_CLIENT 8080
#define PORT_WEB_SERVER 8081
#define SA struct sockaddr

static int c = 0;

// Vérifie que c'est une requête HTTP valide et extrait le path
char *tcp_verify(char *buff)
{
    char method[8], path[64], protocol[16];
    if (sscanf(buff, "%7s %63s %15s", method, path, protocol) == 3)
    {
        if ((strcmp(method, "GET") == 0 || strcmp(method, "POST") == 0) &&
            strncmp(protocol, "HTTP/", 5) == 0)
        {
            printf("\nRequête valide - Méthode: %s Path: %s Protocole: %s\n", method, path, protocol);
            return strdup(path);
        }
    }
    return NULL;
}

// Détermine le Content-Type à partir de l'extension
char* get_type(char *path)
{
    char *ext = strrchr(path, '.');
    if (ext) ext++;

    if (!ext) return "application/octet-stream";

    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) return "text/html";
    if (strcmp(ext, "css")  == 0) return "text/css";
    if (strcmp(ext, "js")   == 0) return "application/javascript";
    if (strcmp(ext, "txt")  == 0) return "text/plain";
    if (strcmp(ext, "json") == 0) return "application/json";
    if (strcmp(ext, "png")  == 0) return "image/png";
    if (strcmp(ext, "jpg")  == 0 || strcmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, "gif")  == 0) return "image/gif";
    if (strcmp(ext, "svg")  == 0) return "image/svg";
    if (strcmp(ext, "mp4")  == 0) return "video/mp4";
    if (strcmp(ext, "pdf")  == 0) return "application/pdf";
    if (strcmp(ext, "zip")  == 0) return "application/zip";

    return "application/octet-stream";
}

// Nouvelle fonction principale : lit tout le body et envoie la réponse HTTP complète
void send_Client_Balencer(int connfd_client, int sock_balence_webSever)
{
    char request_buff[MAX];
    int n = read(connfd_client, request_buff, sizeof(request_buff) - 1);
    if (n <= 0)
    {
        printf("Client déconnecté prématurément\n");
        return;
    }
    request_buff[n] = '\0'; // pour sscanf

    c++;
    printf("\n=*=*=*=*=\nTCP [%d] Requête reçue:\n%s\n=*=*=*=*=\n", c, request_buff);

    char *path = tcp_verify(request_buff);
    if (path == NULL)
    {
        // Requête mal formée
        const char *body = "<h1>400 Bad Request</h1><p>Requête HTTP invalide.</p>";
        char response[1024];
        snprintf(response, sizeof(response),
                 "HTTP/1.1 400 Bad Request\r\n"
                 "Content-Type: text/html\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n\r\n%s",
                 strlen(body), body);
        write(connfd_client, response, strlen(response));
        free(path); // même si NULL, safe
        return;
    }

    // Envoyer le path au webserver
    write(sock_balence_webSever, path, strlen(path));

    // === Étape clé : accumuler tout le body brut ===
    char chunk[MAX];
    size_t total_size = 0;
    size_t capacity = 0;
    char *body = NULL;

    while ((n = read(sock_balence_webSever, chunk, MAX)) > 0)
    {
        // Réallouer si nécessaire

        if (total_size + n > capacity)
        {
            capacity = (capacity == 0) ? MAX : capacity * 2;
            if (capacity < total_size + n) capacity = total_size + n + MAX;
            body = realloc(body, capacity);
            if (!body)
            {
                perror("realloc failed");
                free(path);
                return;
            }
        }
        memcpy(body + total_size, chunk, n);
        total_size += n;
        bzero(chunk,MAX);
    }

    // === Décider du status code ===
    const char *status = "200 OK";
    const char *content_type = get_type(path);

    // Optionnel : détecter un 404 simple (si body très court et contient "404")
    // Tu peux affiner plus tard si besoin
    if (total_size < 100 && strstr(body ? body : "", "404") != NULL)
    {
        status = "404 Not Found";
    }

    // === Construire et envoyer la réponse complète ===
    char header[1024];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n\r\n",
                              status, content_type, total_size);

    // Envoyer header
    write(connfd_client, header, header_len);

    // Envoyer body (si y en a)
    if (total_size > 0 && body)
    {
        write(connfd_client, body, total_size);
    }

    // Nettoyage
    free(path);
    if (body) free(body);

    printf("Réponse envoyée - Status: %s, Taille body: %zu bytes\n", status, total_size);
}

int main()
{
    websocket socket_balence_client = socket_connect(NULL, PORT_CLIENT, 0);
    c = 0;

    while (1)
    {
        printf("================== Load Balancer attend un client ==================\n");
        accept_client(socket_balence_client);

        websocket sock_to_webserver = socket_connect(NULL, PORT_WEB_SERVER, 1);
        connection(sock_to_webserver);

        send_Client_Balencer(socket_balence_client->connfd, sock_to_webserver->socket_in);

        close(socket_balence_client->connfd);
        printf("\n=*=*=*=*=\nTCP[%d] Terminé\n=*=*=*=*=\n", c);
    }

    return 0; // jamais atteint
}