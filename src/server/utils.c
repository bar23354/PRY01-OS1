#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "server.h"

ClientInfo clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_fd = -1;
volatile int server_running = 1;

const char *status_to_str(UserStatus s) {
    switch (s) {
        case STATUS_ACTIVO: return "ACTIVO";
        case STATUS_OCUPADO: return "OCUPADO";
        case STATUS_INACTIVO: return "INACTIVO";
    }
    return "ACTIVO";
}

UserStatus str_to_status(const char *s) {
    if (strcmp(s, "OCUPADO") == 0) return STATUS_OCUPADO;
    if (strcmp(s, "INACTIVO") == 0) return STATUS_INACTIVO;
    return STATUS_ACTIVO;
}

void send_to_client(int sockfd, const char *msg) {
    send(sockfd, msg, strlen(msg), MSG_NOSIGNAL);
}

int find_client_by_name(const char *name) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].username, name) == 0) {
            return i;
        }
    }
    return -1;
}

int find_client_by_ip(const char *ip) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].ip, ip) == 0) {
            return i;
        }
    }
    return -1;
}

void broadcast_msg(const char *msg, int exclude_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].sockfd != exclude_fd) {
            send_to_client(clients[i].sockfd, msg);
        }
    }
}

void remove_client(int idx) {
    if (idx >= 0 && idx < MAX_CLIENTS) {
        if (strlen(clients[idx].username) > 0) {
            printf("[SERVER] Usuario '%s' desconectado.\n",
                   clients[idx].username);
        }
        else {
            printf("[SERVER] Cliente sin registro desconectado.\n");
        }
        close(clients[idx].sockfd);
        clients[idx].active = 0;
        clients[idx].sockfd = -1;
        clients[idx].username[0] = '\0';
        clients[idx].ip[0] = '\0';
    }
}
