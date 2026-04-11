#ifndef CLIENT_H
#define CLIENT_H

#define BUFFER_SIZE 4096

extern int sockfd;
extern volatile int running;

void *receive_handler(void *arg);
void print_help(void);

#endif
