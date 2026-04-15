#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "server.h"

static int is_valid_ipv4(const char *ip) {
    struct in_addr addr;
    return ip && inet_pton(AF_INET, ip, &addr) == 1;
}

static UserStatus server_console_status = STATUS_ACTIVO;

static char *ltrim(char *s) {
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

static void rtrim(char *s) {
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static int starts_with_command(const char *input, const char *command) {
    size_t len = strlen(command);
    return strncmp(input, command, len) == 0 &&
           (input[len] == '\0' || isspace((unsigned char)input[len]));
}

static void build_server_sender_label(char *label, size_t label_size) {
    if (server_console_status == STATUS_ACTIVO) {
        snprintf(label, label_size, "SERVER");
    }
    else {
        snprintf(label, label_size, "SERVER (%s)",
                 status_to_str(server_console_status));
    }
}

static void print_server_help(void) {
    printf("\n");
    printf("+--------------------------------------------------------+\n");
    printf("|                COMANDOS DEL SERVIDOR                  |\n");
    printf("+--------------------------------------------------------+\n");
    printf("|  /broadcast <msg>   - Enviar mensaje a todos          |\n");
    printf("|  /message <msg>     - Alias de /broadcast             |\n");
    printf("|  /msg <user> <msg>  - Mensaje privado a un usuario    |\n");
    printf("|  /status <estado>   - Cambiar status del servidor     |\n");
    printf("|                      (ACTIVO, OCUPADO, INACTIVO)      |\n");
    printf("|  /list o /lis       - Listar usuarios conectados      |\n");
    printf("|  /info <user>       - Info de un usuario              |\n");
    printf("|  /help              - Mostrar esta ayuda              |\n");
    printf("|  /quit              - Cerrar el servidor              |\n");
    printf("+--------------------------------------------------------+\n");
    printf("\n");
}

static void print_server_prompt(void) {
    if (server_running) {
        printf("server> ");
        fflush(stdout);
    }
}

static void log_client_command(const char *username,
                               const char *ip,
                               const char *raw_command) {
    const char *display_name =
        (username && strlen(username) > 0) ? username : "(sin registrar)";
    const char *display_ip = (ip && strlen(ip) > 0) ? ip : "?";
    printf("[SERVER][CMD] %s (%s) -> %s\n", display_name, display_ip,
           raw_command);
}

static void list_users_to_server(void) {
    pthread_mutex_lock(&clients_mutex);
    printf("[SERVER][LOCAL] Usuarios conectados:\n");

    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strlen(clients[i].username) > 0) {
            printf("  %d. %s (%s, %s)\n", ++count, clients[i].username,
                   clients[i].ip, status_to_str(clients[i].status));
        }
    }

    if (count == 0) {
        printf("  (sin usuarios conectados)\n");
    }
    pthread_mutex_unlock(&clients_mutex);
}

static void print_user_info_to_server(const char *target) {
    if (strcmp(target, "SERVER") == 0) {
        printf("[SERVER][LOCAL] Nombre: SERVER\n");
        printf("[SERVER][LOCAL] IP:     local\n");
        printf("[SERVER][LOCAL] Status: %s\n",
               status_to_str(server_console_status));
        return;
    }

    pthread_mutex_lock(&clients_mutex);
    int idx = find_client_by_name(target);
    if (idx < 0) {
        pthread_mutex_unlock(&clients_mutex);
        printf("[SERVER][LOCAL] Usuario '%s' no encontrado.\n", target);
        return;
    }

    printf("[SERVER][LOCAL] Nombre: %s\n", clients[idx].username);
    printf("[SERVER][LOCAL] IP:     %s\n", clients[idx].ip);
    printf("[SERVER][LOCAL] Status: %s\n", status_to_str(clients[idx].status));
    pthread_mutex_unlock(&clients_mutex);
}

static void server_broadcast_message(const char *message) {
    char sender[USERNAME_MAX + 32];
    char payload[BUFFER_SIZE];
    build_server_sender_label(sender, sizeof(sender));

    pthread_mutex_lock(&clients_mutex);
    snprintf(payload, sizeof(payload), "MSG_BROADCAST|%s|%s\n", sender,
             message);
    broadcast_msg(payload, -1);
    pthread_mutex_unlock(&clients_mutex);

    printf("[SERVER][LOCAL] Broadcast enviado: %s\n", message);
}

static void server_private_message(const char *target, const char *message) {
    char sender[USERNAME_MAX + 32];
    char payload[BUFFER_SIZE];
    build_server_sender_label(sender, sizeof(sender));

    pthread_mutex_lock(&clients_mutex);
    int idx = find_client_by_name(target);
    if (idx < 0) {
        pthread_mutex_unlock(&clients_mutex);
        printf("[SERVER][LOCAL] Usuario '%s' no encontrado.\n", target);
        return;
    }

    snprintf(payload, sizeof(payload), "MSG_PRIVATE|%s|%s\n", sender, message);
    send_to_client(clients[idx].sockfd, payload);
    pthread_mutex_unlock(&clients_mutex);

    printf("[SERVER][LOCAL] Mensaje privado enviado a '%s': %s\n", target,
           message);
}

void *inactivity_checker(void *arg) {
    (void)arg;

    while (server_running) {
        sleep(10);
        time_t now = time(NULL);

        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active &&
                clients[i].status != STATUS_INACTIVO &&
                difftime(now, clients[i].last_activity) >= INACTIVITY_SEC) {
                clients[i].status = STATUS_INACTIVO;
                send_to_client(clients[i].sockfd, "STATUS_UPDATE|INACTIVO\n");
                printf("[SERVER] '%s' marcado como INACTIVO por inactividad.\n",
                       clients[i].username);
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    return NULL;
}

void *handle_client(void *arg) {
    int slot = *(int *)arg;
    free(arg);

    int sockfd;
    pthread_mutex_lock(&clients_mutex);
    sockfd = clients[slot].sockfd;
    pthread_mutex_unlock(&clients_mutex);

    char buffer[BUFFER_SIZE];
    int registered = 0;

    while (server_running) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);

        if (bytes <= 0) {
            pthread_mutex_lock(&clients_mutex);
            if (clients[slot].active) {
                if (strlen(clients[slot].username) > 0) {
                    printf("[SERVER] Desconexion inesperada de '%s'.\n",
                           clients[slot].username);
                }
                else {
                    printf("[SERVER] Cliente no registrado desconectado.\n");
                }
                remove_client(slot);
            }
            pthread_mutex_unlock(&clients_mutex);
            break;
        }

        buffer[bytes] = '\0';
        char *newline = strchr(buffer, '\n');
        if (newline) {
            *newline = '\0';
        }

        char raw_command[BUFFER_SIZE];
        strncpy(raw_command, buffer, sizeof(raw_command) - 1);
        raw_command[sizeof(raw_command) - 1] = '\0';

        pthread_mutex_lock(&clients_mutex);
        clients[slot].last_activity = time(NULL);
        if (clients[slot].status == STATUS_INACTIVO) {
            clients[slot].status = STATUS_ACTIVO;
            send_to_client(sockfd, "STATUS_UPDATE|ACTIVO\n");
        }
        char log_name[USERNAME_MAX];
        char log_ip[INET_ADDRSTRLEN];
        strncpy(log_name, clients[slot].username, sizeof(log_name) - 1);
        log_name[sizeof(log_name) - 1] = '\0';
        strncpy(log_ip, clients[slot].ip, sizeof(log_ip) - 1);
        log_ip[sizeof(log_ip) - 1] = '\0';
        pthread_mutex_unlock(&clients_mutex);

        log_client_command(log_name, log_ip, raw_command);

        char *cmd = strtok(buffer, "|");
        if (!cmd) {
            continue;
        }

        if (strcmp(cmd, "REGISTER") == 0) {
            char *name = strtok(NULL, "|");
            char *reported_ip = strtok(NULL, "|");
            if (!name || strlen(name) == 0) {
                send_to_client(sockfd, "ERROR|REGISTER|BAD_FORMAT\n");
                continue;
            }

            const char *effective_ip = clients[slot].ip;
            if (is_valid_ipv4(reported_ip)) {
                effective_ip = reported_ip;
            }

            pthread_mutex_lock(&clients_mutex);

            if (find_client_by_name(name) >= 0) {
                pthread_mutex_unlock(&clients_mutex);
                send_to_client(sockfd, "ERROR|REGISTER|USERNAME_TAKEN\n");
                continue;
            }

            const char *allow_same_ip = getenv("CHAT_ALLOW_SAME_IP");
            int enforce_unique_ip =
                !(allow_same_ip && strcmp(allow_same_ip, "1") == 0);

            if (enforce_unique_ip) {
                int ip_idx = find_client_by_ip(effective_ip);
                if (ip_idx >= 0 && ip_idx != slot) {
                    pthread_mutex_unlock(&clients_mutex);
                    send_to_client(sockfd,
                                   "ERROR|REGISTER|IP_ALREADY_CONNECTED\n");
                    continue;
                }
            }

            strncpy(clients[slot].username, name, USERNAME_MAX - 1);
            clients[slot].username[USERNAME_MAX - 1] = '\0';
            strncpy(clients[slot].ip, effective_ip, INET_ADDRSTRLEN - 1);
            clients[slot].ip[INET_ADDRSTRLEN - 1] = '\0';
            clients[slot].status = STATUS_ACTIVO;
            clients[slot].last_activity = time(NULL);
            registered = 1;

            pthread_mutex_unlock(&clients_mutex);

            send_to_client(sockfd, "OK|REGISTER\n");
            printf("[SERVER] Usuario '%s' registrado desde %s.\n", name,
                   effective_ip);
        }
        else if (strcmp(cmd, "EXIT") == 0) {
            printf("[SERVER][LOG] '%s' solicito salir.\n", log_name);
            send_to_client(sockfd, "OK|EXIT\n");
            pthread_mutex_lock(&clients_mutex);
            remove_client(slot);
            pthread_mutex_unlock(&clients_mutex);
            break;
        }
        else if (!registered) {
            printf("[SERVER][LOG] Cliente no registrado intento usar '%s'.\n",
                   cmd);
            send_to_client(sockfd, "ERROR|GENERAL|NOT_REGISTERED\n");
        }
        else if (strcmp(cmd, "LIST_USERS") == 0) {
            char resp[BUFFER_SIZE];
            strcpy(resp, "OK|LIST_USERS|");

            pthread_mutex_lock(&clients_mutex);
            int first = 1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].active && strlen(clients[i].username) > 0) {
                    if (!first) {
                        strcat(resp, ",");
                    }
                    strcat(resp, clients[i].username);
                    first = 0;
                }
            }
            pthread_mutex_unlock(&clients_mutex);

            strcat(resp, "\n");
            send_to_client(sockfd, resp);
            printf("[SERVER][LOG] Lista de usuarios enviada a '%s'.\n",
                   log_name);
        }
        else if (strcmp(cmd, "GET_USER") == 0) {
            char *target = strtok(NULL, "|");
            if (!target) {
                send_to_client(sockfd, "ERROR|GET_USER|BAD_FORMAT\n");
                continue;
            }

            pthread_mutex_lock(&clients_mutex);
            int idx = find_client_by_name(target);
            if (idx < 0) {
                pthread_mutex_unlock(&clients_mutex);
                send_to_client(sockfd, "ERROR|GET_USER|USER_NOT_FOUND\n");
            }
            else {
                char resp[BUFFER_SIZE];
                snprintf(resp, sizeof(resp), "OK|GET_USER|%s|%s|%s\n",
                         clients[idx].username,
                         clients[idx].ip,
                         status_to_str(clients[idx].status));
                pthread_mutex_unlock(&clients_mutex);
                send_to_client(sockfd, resp);
                printf("[SERVER][LOG] '%s' consulto info de '%s'.\n",
                       log_name, target);
            }
        }
        else if (strcmp(cmd, "SET_STATUS") == 0) {
            char *new_status = strtok(NULL, "|");
            if (!new_status) {
                send_to_client(sockfd, "ERROR|SET_STATUS|BAD_FORMAT\n");
                continue;
            }

            if (strcmp(new_status, "ACTIVO") != 0 &&
                strcmp(new_status, "OCUPADO") != 0 &&
                strcmp(new_status, "INACTIVO") != 0) {
                send_to_client(sockfd, "ERROR|SET_STATUS|INVALID_STATUS\n");
                continue;
            }

            pthread_mutex_lock(&clients_mutex);
            clients[slot].status = str_to_status(new_status);
            pthread_mutex_unlock(&clients_mutex);

            char resp[BUFFER_SIZE];
            snprintf(resp, sizeof(resp), "OK|SET_STATUS|%s\n", new_status);
            send_to_client(sockfd, resp);
            printf("[SERVER][LOG] '%s' cambio su status a %s.\n", log_name,
                   new_status);
        }
        else if (strcmp(cmd, "BROADCAST") == 0) {
            char *message = strtok(NULL, "");
            if (!message) {
                send_to_client(sockfd, "ERROR|BROADCAST|BAD_FORMAT\n");
                continue;
            }

            if (message[0] == '|') {
                message++;
            }

            char bcast[BUFFER_SIZE];
            pthread_mutex_lock(&clients_mutex);
            snprintf(bcast, sizeof(bcast), "MSG_BROADCAST|%s|%s\n",
                     clients[slot].username, message);
            broadcast_msg(bcast, sockfd);
            pthread_mutex_unlock(&clients_mutex);

            send_to_client(sockfd, "OK|BROADCAST\n");
            printf("[SERVER][LOG] Broadcast de '%s': %s\n", log_name, message);
        }
        else if (strcmp(cmd, "PRIVATE") == 0) {
            char *target = strtok(NULL, "|");
            char *message = strtok(NULL, "");
            if (!target || !message) {
                send_to_client(sockfd, "ERROR|PRIVATE|BAD_FORMAT\n");
                continue;
            }

            pthread_mutex_lock(&clients_mutex);
            int idx = find_client_by_name(target);
            if (idx < 0) {
                pthread_mutex_unlock(&clients_mutex);
                send_to_client(sockfd, "ERROR|PRIVATE|USER_NOT_FOUND\n");
            }
            else {
                char priv[BUFFER_SIZE];
                snprintf(priv, sizeof(priv), "MSG_PRIVATE|%s|%s\n",
                         clients[slot].username, message);
                send_to_client(clients[idx].sockfd, priv);
                pthread_mutex_unlock(&clients_mutex);
                send_to_client(sockfd, "OK|PRIVATE\n");
                printf("[SERVER][LOG] Privado de '%s' para '%s': %s\n",
                       log_name, target, message);
            }
        }
        else if (strcmp(cmd, "HELP") == 0) {
            send_to_client(sockfd,
                           "OK|HELP|REGISTER,EXIT,LIST_USERS,GET_USER,"
                           "SET_STATUS,BROADCAST,PRIVATE\n");
            printf("[SERVER][LOG] Ayuda enviada a '%s'.\n", log_name);
        }
        else {
            printf("[SERVER][LOG] Comando invalido de '%s': %s\n", log_name,
                   cmd);
            send_to_client(sockfd, "ERROR|GENERAL|INVALID_COMMAND\n");
        }
    }

    return NULL;
}

void *server_console_loop(void *arg) {
    (void)arg;

    printf("[SERVER] Consola local lista. Escribe /help para ver comandos.\n");
    print_server_prompt();

    char input[BUFFER_SIZE];
    while (server_running && fgets(input, sizeof(input), stdin)) {
        input[strcspn(input, "\n")] = '\0';
        rtrim(input);

        char *line = ltrim(input);
        if (strlen(line) == 0) {
            print_server_prompt();
            continue;
        }

        printf("[SERVER][LOCAL CMD] %s\n", line);

        if (starts_with_command(line, "/broadcast")) {
            char *message = ltrim(line + strlen("/broadcast"));
            if (strlen(message) == 0) {
                printf("Uso: /broadcast <mensaje>\n");
            }
            else {
                server_broadcast_message(message);
            }
        }
        else if (starts_with_command(line, "/message")) {
            char *message = ltrim(line + strlen("/message"));
            if (strlen(message) == 0) {
                printf("Uso: /message <mensaje>\n");
            }
            else {
                server_broadcast_message(message);
            }
        }
        else if (starts_with_command(line, "/msg")) {
            char *rest = ltrim(line + strlen("/msg"));
            char *target = strtok(rest, " ");
            char *message = strtok(NULL, "");
            if (!target || !message) {
                printf("Uso: /msg <usuario> <mensaje>\n");
            }
            else {
                server_private_message(target, message);
            }
        }
        else if (starts_with_command(line, "/status")) {
            char *new_status = ltrim(line + strlen("/status"));
            if (strcmp(new_status, "ACTIVO") != 0 &&
                strcmp(new_status, "OCUPADO") != 0 &&
                strcmp(new_status, "INACTIVO") != 0) {
                printf("Uso: /status <ACTIVO|OCUPADO|INACTIVO>\n");
            }
            else {
                server_console_status = str_to_status(new_status);
                printf("[SERVER][LOCAL] Status del servidor cambiado a %s.\n",
                       new_status);
            }
        }
        else if (strcmp(line, "/list") == 0 || strcmp(line, "/lis") == 0) {
            list_users_to_server();
        }
        else if (starts_with_command(line, "/info")) {
            char *target = ltrim(line + strlen("/info"));
            if (strlen(target) == 0) {
                printf("Uso: /info <usuario>\n");
            }
            else {
                print_user_info_to_server(target);
            }
        }
        else if (strcmp(line, "/help") == 0) {
            print_server_help();
        }
        else if (strcmp(line, "/quit") == 0 || strcmp(line, "/exit") == 0) {
            handle_sigint(0);
            break;
        }
        else if (line[0] != '/') {
            server_broadcast_message(line);
        }
        else {
            printf("Comando no reconocido. Escribe /help para ver los comandos.\n");
        }

        print_server_prompt();
    }

    return NULL;
}
