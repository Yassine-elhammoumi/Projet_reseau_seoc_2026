#include "file_descriptor.h"



/**
 * @brief Détermine si le buffer reçu contient un file descriptor.
 *
 * @param buf  Buffer reçu.
 * @param len  Taille des données reçues.
 *
 * @return 1 si le buffer commence par un file descriptor valide, 0 sinon.
 */
int is_descriptor(void *buf) {
    int size;
    char dash;

    int m = sscanf(buf, "%d%c", &size, &dash);

    if (m == 2 && dash == '-') {
        printf("OK: size est un entier (%d)\n", size);
        return 1;
    }

    return 0;
}


/**
 * @brief Libère les ressources associées au contexte d'un file descriptor.
 * 
 * @param file_descriptor tableau des contextes de file descriptor
 * @param fd indice du socket dont on veut libérer le contexte
 */
void cleanup_context(file_descriptor_context *file_descriptor) {
    file_descriptor_context *context = file_descriptor;
    if (!context) return;

    if (context->backend_fd) {
        if (context->backend_fd->socket_in > 0)
            close(context->backend_fd->socket_in);

        free(context->backend_fd);
        context->backend_fd = NULL;
    }

    if (context->entries) {
        free(context->entries);
        context->entries = NULL;
    }
    free(context);
    file_descriptor = NULL;
}


/**
 * @brief Crée et initialise un contexte de file descriptor à partir d'un buffer.
 * 
 * @param file_descriptor tableau des contextes de file descriptor
 * @param fd indice du socket ayant envoyé le descriptor
 * @param buf buffer contenant le file descriptor
 * @param len taille du buffer
 * @return 1 en cas de succès, -1 en cas d'erreur de format, 0 en cas d'erreur d'allocation
 */
int create_file_descriptor_context(file_descriptor_context **file_descriptor,
                                   const void *buf, size_t len)
{
    // Allocation du contexte
    *file_descriptor = malloc(sizeof(**file_descriptor));
    if (!*file_descriptor) {
        perror("malloc");
        return -1;
    }

    printf("Est-ce un file descriptor : %s ?\n", (char *)buf);

    char size_str[16];  // ⚠️ cohérent avec %15[^-]

    int offset = 0;
    int parsed = sscanf(buf, "%15[^-]-%n", size_str, &offset);
    if (parsed != 1) {
        printf("Format invalide\n");
        free(*file_descriptor);
        return -1;
    }

    int entry_number = atoi(size_str);

    // allocation tableau des entrées
    (*file_descriptor)->entries =
        malloc(entry_number * sizeof(*(*file_descriptor)->entries));
    if (!(*file_descriptor)->entries) {
        perror("malloc");
        free(*file_descriptor);
        return -1;
    }

    // initialisation des champs du contexte
    (*file_descriptor)->entry_count = entry_number;
    (*file_descriptor)->current_index = 0;
    (*file_descriptor)->bytes_fetched_for_index = 0;
    (*file_descriptor)->backend_fd = NULL;
    (*file_descriptor)->backend_remaining_bytes = 0;

    // parser les entrées
    int parse_result = parse_entries(
        (*file_descriptor)->entries,
        (*file_descriptor)->entry_count,
        (char *)buf + offset
    );

    printf("parse_result est %d\n", parse_result);

    if (parse_result < 0) {
        free((*file_descriptor)->entries);
        free(*file_descriptor);
        return -1;
    }

    return 1;
}


/**
 * @brief Parse les entrées du file descriptor depuis un buffer.
 * 
 * @param entries tableau d'entrées à remplir
 * @param count nombre d'entrées à parser
 * @param ptr pointeur vers le début des entrées dans le buffer
 * @return 1 en cas de succès, -1 en cas d’erreur de format
 */
int parse_entries(file_descriptor_entry *entries, size_t count, const char *ptr) {
    char ip[16];
    char port_str[6];
    char offset_str[21];
    char length_str[21];



    int parsed   = 0;
    int consumed = 0;
    for (int i = 0; i < (int) count; i++) {
        parsed = sscanf(ptr,
            "%15[^-]-%5[^-]-%20[^-]-%20[^-]-%n",
            ip, port_str, offset_str, length_str, &consumed);

        printf("Bloc %d / %d → IP=%s PORT=%s OFFSET=%s LENGTH=%s\n",i,(int) count, ip, port_str, offset_str, length_str);

        if (parsed != 4) {
            printf("Erreur parsing bloc %d\n", i);
            return -1;
        }
        memset(entries[i].ip, 0, sizeof(entries[i].ip));
        strncpy(entries[i].ip, ip, sizeof(entries[i].ip) - 1);
        entries[i].port   = (int) strtol(port_str, NULL, 10);
        entries[i].length = strtol(length_str, NULL, 10);
        entries[i].offset = strtol(offset_str, NULL, 10);


        
        if (consumed==0){
            break;
        }
        ptr += consumed;
    }
    return 1;
}

