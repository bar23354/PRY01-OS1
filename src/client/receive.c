#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "client.h"

void *receive_handler(void *arg) {
    (void)arg;
    char buffer[BUFFER_SIZE];

    while (running) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            if (running) {
                printf("\n[!] Se perdio la conexion con el servidor.\n");
                running = 0;
            }
            break;
        }

        buffer[bytes] = '\0';

        char *save_line = NULL;
        char *line = strtok_r(buffer, "\n", &save_line);
        while (line) {
            if (strncmp(line, "MSG_BROADCAST|", 14) == 0) {
                char *rest = line + 14;
                char *sep = strchr(rest, '|');
                if (sep) {
                    *sep = '\0';
                    printf("\n[General] %s: %s\n", rest, sep + 1);
                }
                else {
                    printf("\n%s\n", line);
                }
            }
            else if (strncmp(line, "MSG_PRIVATE|", 12) == 0) {
                char *rest = line + 12;
                char *sep = strchr(rest, '|');
                if (sep) {
                    *sep = '\0';
                    printf("\n[Privado de %s]: %s\n", rest, sep + 1);
                }
                else {
                    printf("\n%s\n", line);
                }
            }
            else if (strncmp(line, "STATUS_UPDATE|", 14) == 0) {
                char *new_status = line + 14;
                printf("\n[Sistema] Tu status cambio a: %s\n", new_status);
            }
            else if (strncmp(line, "OK|", 3) == 0) {
                char *rest = line + 3;

                if (strncmp(rest, "REGISTER", 8) == 0) {
                    printf("[OK] Registrado exitosamente.\n");
                }
                else if (strncmp(rest, "EXIT", 4) == 0) {
                    printf("[OK] Desconectado del servidor.\n");
                    running = 0;
                }
                else if (strncmp(rest, "LIST_USERS|", 11) == 0) {
                    char *users = rest + 11;
                    printf("\n=== Usuarios conectados ===\n");
                    char *save_users = NULL;
                    char *u = strtok_r(users, ",", &save_users);
                    int count = 0;
                    while (u) {
                        printf("  %d. %s\n", ++count, u);
                        u = strtok_r(NULL, ",", &save_users);
                    }
                    printf("===========================\n");
                }
                else if (strncmp(rest, "GET_USER|", 9) == 0) {
                    char *info = rest + 9;
                    char *save_info = NULL;
                    char *name = strtok_r(info, "|", &save_info);
                    char *ip = strtok_r(NULL, "|", &save_info);
                    char *st = strtok_r(NULL, "|", &save_info);
                    printf("\n--- Info de usuario ---\n");
                    printf("  Nombre: %s\n", name ? name : "?");
                    printf("  IP:     %s\n", ip ? ip : "?");
                    printf("  Status: %s\n", st ? st : "?");
                    printf("-----------------------\n");
                }
                else if (strncmp(rest, "SET_STATUS|", 11) == 0) {
                    char *st = rest + 11;
                    printf("[OK] Status cambiado a: %s\n", st);
                }
                else if (strncmp(rest, "BROADCAST", 9) == 0) {
                }
                else if (strncmp(rest, "PRIVATE", 7) == 0) {
                    printf("[OK] Mensaje privado enviado.\n");
                }
                else if (strncmp(rest, "HELP|", 5) == 0) {
                    char *cmds = rest + 5;
                    printf("\n=== Comandos disponibles ===\n");
                    printf("  %s\n", cmds);
                    printf("============================\n");
                }
                else {
                    printf("[Servidor] %s\n", line);
                }
            }
            else if (strncmp(line, "ERROR|", 6) == 0) {
                printf("[ERROR] %s\n", line + 6);
                if (strstr(line, "REGISTER") && strstr(line, "USERNAME_TAKEN")) {
                    printf("El nombre de usuario ya esta en uso. Saliendo...\n");
                    running = 0;
                }
                if (strstr(line, "IP_ALREADY_CONNECTED")) {
                    printf("Ya hay un usuario conectado desde esta IP. Saliendo...\n");
                    running = 0;
                }
                if (strstr(line, "SERVER") && strstr(line, "SHUTDOWN")) {
                    printf("El servidor se cerro.\n");
                    running = 0;
                }
            }
            else {
                printf("[Servidor] %s\n", line);
            }

            if (running) {
                printf("> ");
                fflush(stdout);
            }

            line = strtok_r(NULL, "\n", &save_line);
        }
    }

    return NULL;
}
