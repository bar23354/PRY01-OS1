# Makefile - Chat Multithread
# CC3064 Sistemas Operativos

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

SERVER_SRCS = server.c server_utils.c server_threads.c
CLIENT_SRCS = client.c client_receive.c client_ui.c

all: chat_server chat_client

chat_server: $(SERVER_SRCS)
	$(CC) $(CFLAGS) -o chat_server $(SERVER_SRCS)

chat_client: $(CLIENT_SRCS)
	$(CC) $(CFLAGS) -o chat_client $(CLIENT_SRCS)

clean:
	rm -f chat_server chat_client
