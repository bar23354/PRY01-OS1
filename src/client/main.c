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
#include <ctype.h>
#include <sys/wait.h>

#include "client.h"

int sockfd = -1;
volatile int running = 1;

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

static int is_valid_ipv4(const char *ip) {
    struct sockaddr_in addr;
    return ip && inet_pton(AF_INET, ip, &addr.sin_addr) == 1;
}

static int capture_process_output(const char *program,
                                  char *const argv[],
                                  char *output,
                                  size_t output_size) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        return 0;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return 0;
    }

    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execvp(program, argv);
        _exit(127);
    }

    close(pipefd[1]);

    size_t total = 0;
    ssize_t bytes;
    while ((bytes = read(pipefd[0], output + total,
                         output_size - total - 1)) > 0) {
        total += (size_t)bytes;
        if (total >= output_size - 1) {
            break;
        }
    }
    close(pipefd[0]);
    output[total] = '\0';

    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int detect_socket_local_ip(int fd, char *ip, size_t ip_size) {
    struct sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);
    if (getsockname(fd, (struct sockaddr *)&local_addr, &len) != 0) {
        return 0;
    }

    return inet_ntop(AF_INET, &local_addr.sin_addr, ip, ip_size) != NULL;
}

static int detect_wsl_windows_ip(const char *server_ip,
                                 int port,
                                 char *ip,
                                 size_t ip_size) {
    if (!getenv("WSL_DISTRO_NAME")) {
        return 0;
    }

    char ps_command[BUFFER_SIZE];
    snprintf(
        ps_command,
        sizeof(ps_command),
        "$ErrorActionPreference='Stop'; "
        "$socket = New-Object Net.Sockets.Socket("
        "[Net.Sockets.AddressFamily]::InterNetwork,"
        "[Net.Sockets.SocketType]::Dgram,"
        "[Net.Sockets.ProtocolType]::Udp); "
        "try { "
        "$socket.Connect('%s', %d); "
        "[Console]::WriteLine($socket.LocalEndPoint.Address.ToString()) "
        "} finally { $socket.Close() }",
        server_ip,
        port);

    char output[BUFFER_SIZE];
    char *const argv[] = {
        "powershell.exe",
        "-NoProfile",
        "-Command",
        ps_command,
        NULL
    };

    if (!capture_process_output("powershell.exe", argv, output, sizeof(output))) {
        return 0;
    }

    char *trimmed = ltrim(output);
    rtrim(trimmed);

    if (!is_valid_ipv4(trimmed)) {
        return 0;
    }

    strncpy(ip, trimmed, ip_size - 1);
    ip[ip_size - 1] = '\0';
    return 1;
}

static void resolve_client_ip(int fd,
                              const char *server_ip,
                              int port,
                              char *ip,
                              size_t ip_size) {
    ip[0] = '\0';

    const char *override_ip = getenv("CHAT_CLIENT_IP");
    if (is_valid_ipv4(override_ip)) {
        strncpy(ip, override_ip, ip_size - 1);
        ip[ip_size - 1] = '\0';
        return;
    }

    if (detect_wsl_windows_ip(server_ip, port, ip, ip_size)) {
        return;
    }

    if (detect_socket_local_ip(fd, ip, ip_size) && is_valid_ipv4(ip)) {
        return;
    }

    ip[0] = '\0';
}

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

    char client_ip[INET_ADDRSTRLEN] = {0};
    resolve_client_ip(sockfd, server_ip, port, client_ip, sizeof(client_ip));

    char reg_msg[BUFFER_SIZE];
    if (strlen(client_ip) > 0) {
        snprintf(reg_msg, sizeof(reg_msg), "REGISTER|%s|%s\n",
                 username, client_ip);
    }
    else {
        snprintf(reg_msg, sizeof(reg_msg), "REGISTER|%s\n", username);
    }
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
        rtrim(input);

        char *line = ltrim(input);

        if (strlen(line) == 0) {
            printf("> ");
            fflush(stdout);
            continue;
        }

        char cmd[BUFFER_SIZE];

        if (starts_with_command(line, "/broadcast")) {
            char *msg = ltrim(line + strlen("/broadcast"));
            if (strlen(msg) == 0) {
                printf("Uso: /broadcast <mensaje>\n");
            }
            else {
                snprintf(cmd, sizeof(cmd), "BROADCAST|%s\n", msg);
                send(sockfd, cmd, strlen(cmd), 0);
            }
        }
        else if (starts_with_command(line, "/message") ||
                 starts_with_command(line, "/messagge") ||
                 starts_with_command(line, "/mensaje")) {
            char *msg = strchr(line, ' ');
            if (!msg) {
                printf("Uso: /message <mensaje>\n");
            }
            else {
                msg = ltrim(msg);
                snprintf(cmd, sizeof(cmd), "BROADCAST|%s\n", msg);
                send(sockfd, cmd, strlen(cmd), 0);
            }
        }
        else if (starts_with_command(line, "/msg")) {
            char *rest = ltrim(line + strlen("/msg"));
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
        else if (starts_with_command(line, "/status")) {
            char *st = ltrim(line + strlen("/status"));
            if (strlen(st) == 0) {
                printf("Uso: /status <ACTIVO|OCUPADO|INACTIVO>\n");
            }
            else {
                snprintf(cmd, sizeof(cmd), "SET_STATUS|%s\n", st);
                send(sockfd, cmd, strlen(cmd), 0);
            }
        }
        else if (strcmp(line, "/list") == 0 || strcmp(line, "/lis") == 0) {
            snprintf(cmd, sizeof(cmd), "LIST_USERS\n");
            send(sockfd, cmd, strlen(cmd), 0);
        }
        else if (starts_with_command(line, "/info")) {
            char *target = ltrim(line + strlen("/info"));
            if (strlen(target) == 0) {
                printf("Uso: /info <usuario>\n");
            }
            else {
                snprintf(cmd, sizeof(cmd), "GET_USER|%s\n", target);
                send(sockfd, cmd, strlen(cmd), 0);
            }
        }
        else if (strcmp(line, "/help") == 0) {
            print_help();
        }
        else if (strcmp(line, "/quit") == 0 || strcmp(line, "/exit") == 0) {
            snprintf(cmd, sizeof(cmd), "EXIT\n");
            send(sockfd, cmd, strlen(cmd), 0);
            usleep(200000);
            running = 0;
            break;
        }
        else if (line[0] != '/') {
            snprintf(cmd, sizeof(cmd), "BROADCAST|%s\n", line);
            send(sockfd, cmd, strlen(cmd), 0);
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
