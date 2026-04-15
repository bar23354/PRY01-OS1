/*
 *   Punto de entrada del servidor de chat multithread.
 *   La logica de protocolo, utilidades y hilos se encuentra en modulos
 *   separados para facilitar mantenimiento y pruebas.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "server.h"

void handle_sigint(int sig) {
    (void)sig;
    printf("\n[SERVER] Cerrando servidor...\n");
    server_running = 0;
    if (server_fd >= 0) {
        close(server_fd);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Puerto invalido.\n");
        return EXIT_FAILURE;
    }

    signal(SIGINT, handle_sigint);
    signal(SIGPIPE, SIG_IGN);

    memset(clients, 0, sizeof(clients));
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].sockfd = -1;
        clients[i].active = 0;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("[SERVER] Escuchando en puerto %d...\n", port);
    printf("[SERVER] Tiempo de inactividad: %d segundos.\n", INACTIVITY_SEC);

    pthread_t inact_thread;
    if (pthread_create(&inact_thread, NULL, inactivity_checker, NULL) != 0) {
        perror("pthread_create");
        close(server_fd);
        return EXIT_FAILURE;
    }
    pthread_detach(inact_thread);

    pthread_t console_thread;
    if (pthread_create(&console_thread, NULL, server_console_loop, NULL) != 0) {
        perror("pthread_create");
        close(server_fd);
        return EXIT_FAILURE;
    }
    pthread_detach(console_thread);

    while (server_running) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int cli_fd = accept(server_fd, (struct sockaddr *)&cli_addr, &cli_len);

        if (cli_fd < 0) {
            if (!server_running) {
                break;
            }
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("[SERVER] Nueva conexion desde %s\n", client_ip);

        pthread_mutex_lock(&clients_mutex);
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) {
                slot = i;
                break;
            }
        }

        if (slot < 0) {
            pthread_mutex_unlock(&clients_mutex);
            send_to_client(cli_fd, "ERROR|REGISTER|SERVER_FULL\n");
            close(cli_fd);
            continue;
        }

        clients[slot].sockfd = cli_fd;
        strncpy(clients[slot].ip, client_ip, INET_ADDRSTRLEN - 1);
        clients[slot].ip[INET_ADDRSTRLEN - 1] = '\0';
        clients[slot].status = STATUS_ACTIVO;
        clients[slot].last_activity = time(NULL);
        clients[slot].active = 1;
        clients[slot].username[0] = '\0';

        int *slot_ptr = malloc(sizeof(int));
        if (!slot_ptr) {
            send_to_client(cli_fd, "ERROR|SERVER|INTERNAL\n");
            close(cli_fd);
            clients[slot].active = 0;
            clients[slot].sockfd = -1;
            clients[slot].username[0] = '\0';
            clients[slot].ip[0] = '\0';
            pthread_mutex_unlock(&clients_mutex);
            continue;
        }

        *slot_ptr = slot;
        if (pthread_create(&clients[slot].thread_id, NULL, handle_client,
                           slot_ptr) != 0) {
            perror("pthread_create");
            free(slot_ptr);
            send_to_client(cli_fd, "ERROR|SERVER|INTERNAL\n");
            close(cli_fd);
            clients[slot].active = 0;
            clients[slot].sockfd = -1;
            clients[slot].username[0] = '\0';
            clients[slot].ip[0] = '\0';
            pthread_mutex_unlock(&clients_mutex);
            continue;
        }

        pthread_detach(clients[slot].thread_id);
        pthread_mutex_unlock(&clients_mutex);
    }

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            send_to_client(clients[i].sockfd, "ERROR|SERVER|SHUTDOWN\n");
            close(clients[i].sockfd);
            clients[i].active = 0;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (server_fd >= 0) {
        close(server_fd);
    }
    printf("[SERVER] Servidor cerrado.\n");

    return 0;
}
