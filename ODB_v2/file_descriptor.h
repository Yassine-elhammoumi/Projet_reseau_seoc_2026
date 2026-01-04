#ifndef FILE_DESCRIPTOR
#define FILE_DESCRIPTOR

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include "websocket.h"

#define MAX_FD 2048

typedef struct {
    char ip[16]; // adresse IP sous forme de chaîne (IPv4)
    int port;     // port du backend
    long offset;   // offset réel dans le fichier
    size_t length;   // longueur des données à lire
} file_descriptor_entry;

typedef struct {
    file_descriptor_entry *entries;   // tableau des entrées
    size_t entry_count; //nombre d'entrées
    size_t current_index;   // index de l'entrée courante
    uint64_t bytes_fetched_for_index; // offset dans l'entrée courante
    websocket backend_fd;              // socket connecté au backend
    size_t backend_remaining_bytes;     // bytes attendus du backend pour l'entrée courante
} file_descriptor_context;

int is_descriptor(void *buf);

void cleanup_context(file_descriptor_context *file_descriptor);

int create_file_descriptor_context(file_descriptor_context **file_descriptor, const void *buf, size_t len);

int parse_entries(file_descriptor_entry *entries, size_t count, const char *ptr);

#endif