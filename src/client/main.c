/*
 *   Punto de entrada del cliente de chat.
 *   La recepcion concurrente y la ayuda se encuentran en modulos separados
 *   para mantener este archivo enfocado en flujo principal y comandos.
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

#include "client.h"

int sockfd = -1;
volatile int running = 1;

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <nombre_usuario> <IP_servidor> <puerto>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    char *username = argv[1];
    char *server_ip = argv[2];
    int port = atoi(argv[3]);

    if (strlen(username) == 0 || strlen(username) > 63) {
        fprintf(stderr, "Nombre de usuario invalido (1-63 caracteres).\n");
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Direccion IP invalida: %s\n", server_ip);
        close(sockfd);
        return EXIT_FAILURE;
    }

    printf("Conectando a %s:%d...\n", server_ip, port);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return EXIT_FAILURE;
    }

    printf("Conectado. Registrando como '%s'...\n", username);

    char reg_msg[BUFFER_SIZE];
    snprintf(reg_msg, sizeof(reg_msg), "REGISTER|%s\n", username);
    send(sockfd, reg_msg, strlen(reg_msg), 0);

    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_handler, NULL) != 0) {
        perror("pthread_create");
        close(sockfd);
        return EXIT_FAILURE;
    }

    usleep(300000);

    if (!running) {
        close(sockfd);
        return EXIT_FAILURE;
    }

    print_help();
    printf("> ");
    fflush(stdout);

    char input[BUFFER_SIZE];
    while (running && fgets(input, sizeof(input), stdin)) {
        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0) {
            printf("> ");
            fflush(stdout);
            continue;
        }

        char cmd[BUFFER_SIZE];

        if (strncmp(input, "/broadcast ", 11) == 0) {
            char *msg = input + 11;
            snprintf(cmd, sizeof(cmd), "BROADCAST|%s\n", msg);
            send(sockfd, cmd, strlen(cmd), 0);
        }
        else if (strncmp(input, "/msg ", 5) == 0) {
            char *rest = input + 5;
            char *target = strtok(rest, " ");
            char *msg = strtok(NULL, "");
            if (target && msg) {
                snprintf(cmd, sizeof(cmd), "PRIVATE|%s|%s\n", target, msg);
                send(sockfd, cmd, strlen(cmd), 0);
            }
            else {
                printf("Uso: /msg <usuario> <mensaje>\n");
            }
        }
        else if (strncmp(input, "/status ", 8) == 0) {
            char *st = input + 8;
            snprintf(cmd, sizeof(cmd), "SET_STATUS|%s\n", st);
            send(sockfd, cmd, strlen(cmd), 0);
        }
        else if (strcmp(input, "/list") == 0) {
            snprintf(cmd, sizeof(cmd), "LIST_USERS\n");
            send(sockfd, cmd, strlen(cmd), 0);
        }
        else if (strncmp(input, "/info ", 6) == 0) {
            char *target = input + 6;
            snprintf(cmd, sizeof(cmd), "GET_USER|%s\n", target);
            send(sockfd, cmd, strlen(cmd), 0);
        }
        else if (strcmp(input, "/help") == 0) {
            print_help();
        }
        else if (strcmp(input, "/quit") == 0 || strcmp(input, "/exit") == 0) {
            snprintf(cmd, sizeof(cmd), "EXIT\n");
            send(sockfd, cmd, strlen(cmd), 0);
            usleep(200000);
            running = 0;
            break;
        }
        else if (input[0] != '/') {
            send(sockfd, "BROADCAST|", 10, 0);
            send(sockfd, input, strlen(input), 0);
            send(sockfd, "\n", 1, 0);
        }
        else {
            printf("Comando no reconocido. Escribe /help para ver los comandos.\n");
        }

        if (running) {
            printf("> ");
            fflush(stdout);
        }
    }

    running = 0;
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    pthread_join(recv_thread, NULL);
    printf("Sesion terminada.\n");

    return 0;
}
