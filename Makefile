# Makefile - Chat Multithread
# CC3064 Sistemas Operativos

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
INCLUDES = -Iinclude

BIN_DIR = bin

SERVER_SRCS = src/server/main.c src/server/utils.c src/server/threads.c
CLIENT_SRCS = src/client/main.c src/client/receive.c src/client/ui.c

SERVER_BIN = $(BIN_DIR)/chat_server
CLIENT_BIN = $(BIN_DIR)/chat_client

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRCS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(SERVER_BIN) $(SERVER_SRCS)

$(CLIENT_BIN): $(CLIENT_SRCS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(CLIENT_BIN) $(CLIENT_SRCS)

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)
