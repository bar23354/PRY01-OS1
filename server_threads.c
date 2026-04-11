#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "server.h"

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

        pthread_mutex_lock(&clients_mutex);
        clients[slot].last_activity = time(NULL);
        if (clients[slot].status == STATUS_INACTIVO) {
            clients[slot].status = STATUS_ACTIVO;
            send_to_client(sockfd, "STATUS_UPDATE|ACTIVO\n");
        }
        pthread_mutex_unlock(&clients_mutex);

        char *cmd = strtok(buffer, "|");
        if (!cmd) {
            continue;
        }

        if (strcmp(cmd, "REGISTER") == 0) {
            char *name = strtok(NULL, "|");
            if (!name || strlen(name) == 0) {
                send_to_client(sockfd, "ERROR|REGISTER|BAD_FORMAT\n");
                continue;
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
                int ip_idx = find_client_by_ip(clients[slot].ip);
                if (ip_idx >= 0 && ip_idx != slot) {
                    pthread_mutex_unlock(&clients_mutex);
                    send_to_client(sockfd,
                                   "ERROR|REGISTER|IP_ALREADY_CONNECTED\n");
                    continue;
                }
            }

            strncpy(clients[slot].username, name, USERNAME_MAX - 1);
            clients[slot].username[USERNAME_MAX - 1] = '\0';
            clients[slot].status = STATUS_ACTIVO;
            clients[slot].last_activity = time(NULL);
            registered = 1;

            pthread_mutex_unlock(&clients_mutex);

            send_to_client(sockfd, "OK|REGISTER\n");
            printf("[SERVER] Usuario '%s' registrado.\n", name);
        }
        else if (strcmp(cmd, "EXIT") == 0) {
            send_to_client(sockfd, "OK|EXIT\n");
            pthread_mutex_lock(&clients_mutex);
            remove_client(slot);
            pthread_mutex_unlock(&clients_mutex);
            break;
        }
        else if (!registered) {
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
            }
        }
        else if (strcmp(cmd, "HELP") == 0) {
            send_to_client(sockfd,
                           "OK|HELP|REGISTER,EXIT,LIST_USERS,GET_USER,"
                           "SET_STATUS,BROADCAST,PRIVATE\n");
        }
        else {
            send_to_client(sockfd, "ERROR|GENERAL|INVALID_COMMAND\n");
        }
    }

    return NULL;
}
