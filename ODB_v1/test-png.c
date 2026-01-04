#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8060
#define BUFF_SIZE 4096
#define ROOT_PATH "file"   // dossier contenant les fichiers

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
    if (strcmp(ext, ".xml")  == 0) return "application/xml";
    if (strcmp(ext, ".json") == 0) return "application/json";
    
    // image
    if (strcmp(ext, "png")  == 0) return "image/png";
    if (strcmp(ext, "jpg")  == 0 || strcmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, "gif")  == 0) return "image/gif";
    if (strcmp(ext, "svg")  == 0) return "image/svg";

    // audio
    if (strcmp(ext, ".mp3")  == 0) return "audio/mpeg";
    if (strcmp(ext, ".wav")  == 0) return "audio/wav";
    if (strcmp(ext, ".flac") == 0) return "audio/flac";

    // video
    if (strcmp(ext, ".mp4")  == 0) return "video/mp4";
    if (strcmp(ext, ".webm") == 0) return "video/webm";
    if (strcmp(ext, ".avi")  == 0) return "video/x-msvideo";

    // document
    if (strcmp(ext, ".pdf")  == 0) return "application/pdf";
    if (strcmp(ext, ".zip")  == 0) return "application/zip";
    if (strcmp(ext, ".gz")   == 0) return "application/gzip";
    if (strcmp(ext, ".tar")  == 0) return "application/x-tar";
    return "application/octet-stream";
}
void send_file(int client, const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        const char *msg =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 13\r\n\r\n"
            "404 Not Found";
        write(client, msg, strlen(msg));
        return;
    }

    // obtenir taille
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    // type MIME
    char *mime = get_type((char *)filepath);

    // envoyer header HTTP
    char header[256];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n\r\n",
        mime, size
    );
    write(client, header, hlen);

    // envoyer fichier binaire
    char buffer[BUFF_SIZE];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        write(client, buffer, n);
    }

    fclose(fp);
}

int main() {
    int server, client;
    struct sockaddr_in addr;
    char request[1024];

    // créer socket
    server = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server, (struct sockaddr*)&addr, sizeof(addr));
    listen(server, 5);

    printf("Server running on http://localhost:%d\n", PORT);

    while (1) {
        socklen_t len = sizeof(addr);
        client = accept(server, (struct sockaddr*)&addr, &len);

        // lire la requête GET
        bzero(request, sizeof(request));
        read(client, request, sizeof(request));

        // extraire le fichier demandé
        char method[8], path[256];
        sscanf(request, "%s %s", method, path);

        // si GET / → GET /index.html
        if (strcmp(path, "/") == 0)
            strcpy(path, "/index.html");

        // construire fullpath : ROOT_PATH + path
        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s%s", ROOT_PATH, path);

        // envoyer
        send_file(client, fullpath);

        close(client);
    }

    close(server);
    return 0;
}
