#ifndef SERVER_H
#define SERVER_H

#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <netinet/in.h>

#define MAX_CLIENTS 50
#define BUFFER_SIZE 4096
#define USERNAME_MAX 64
#define INACTIVITY_SEC 60

typedef enum {
    STATUS_ACTIVO,
    STATUS_OCUPADO,
    STATUS_INACTIVO
} UserStatus;

typedef struct {
    int sockfd;
    char username[USERNAME_MAX];
    char ip[INET_ADDRSTRLEN];
    UserStatus status;
    time_t last_activity;
    int active;
    pthread_t thread_id;
} ClientInfo;

extern ClientInfo clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;
extern int server_fd;
extern volatile int server_running;

const char *status_to_str(UserStatus s);
UserStatus str_to_status(const char *s);
void send_to_client(int sockfd, const char *msg);
int find_client_by_name(const char *name);
int find_client_by_ip(const char *ip);
void broadcast_msg(const char *msg, int exclude_fd);
void remove_client(int idx);

void *inactivity_checker(void *arg);
void *handle_client(void *arg);
void *server_console_loop(void *arg);
void handle_sigint(int sig);

#endif
