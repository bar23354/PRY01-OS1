# PRY01-OS1

Proyecto 01 de Sistemas Operativos: Chat multithread

## Integrantes

- Anggelie Lizeth Velasquez Asencio - 221181
- Mia Alejandra Fuentes Merida - 23775
- Roberto Jose Barreda Siekavizza - 23354

## Descripcion

Este proyecto implementa un sistema de chat en C con:

- Servidor concurrente con `pthread` (un hilo por cliente).
- Cliente de consola con hilo receptor para recibir mensajes mientras el usuario escribe.
- Protocolo de texto simple para registro, mensajes generales/privados y gestion de estado.

## Funcionalidades principales

- Registro de usuarios con validacion de nombre e IP.
- Mensajes globales (`BROADCAST`) y privados (`PRIVATE`).
- Consulta de usuarios conectados e informacion individual.
- Cambio de estado de usuario (`ACTIVO`, `OCUPADO`, `INACTIVO`).
- Cambio automatico a `INACTIVO` tras inactividad.
- Cierre ordenado del servidor con `SIGINT`.

## Estructura modular

- `include/`: headers compartidos.
	- `include/server.h`
	- `include/client.h`
- `src/server/`: codigo fuente del servidor.
	- `src/server/main.c`
	- `src/server/utils.c`
	- `src/server/threads.c`
- `src/client/`: codigo fuente del cliente.
	- `src/client/main.c`
	- `src/client/receive.c`
	- `src/client/ui.c`
- `bin/`: ejecutables generados (`chat_server`, `chat_client`).

## Requisitos

- Linux o WSL (usa headers POSIX: `pthread`, `unistd`, `sys/socket.h`, etc.).
- `gcc`
- `make`

## Compilacion

```bash
make
```

Si no tienes `make`:

```bash
mkdir -p bin
gcc -Wall -Wextra -pthread -g -Iinclude -o bin/chat_server src/server/main.c src/server/utils.c src/server/threads.c
gcc -Wall -Wextra -pthread -g -Iinclude -o bin/chat_client src/client/main.c src/client/receive.c src/client/ui.c
```

## Ejecucion

1. Levantar el servidor:

```bash
./bin/chat_server <puerto>
```

Modo pruebas local (permite varios clientes desde la misma IP):

```bash
CHAT_ALLOW_SAME_IP=1 ./bin/chat_server <puerto>
```

2. Conectar clientes (en otras terminales):

```bash
./bin/chat_client <usuario> <ip_servidor> <puerto>
```

Ejemplo local:

```bash
./bin/chat_server 8080
./bin/chat_client Ana 127.0.0.1 8080
```

## Comandos del cliente

- `/broadcast <mensaje>`
- `/msg <usuario> <mensaje>`
- `/status <ACTIVO|OCUPADO|INACTIVO>`
- `/list`
- `/info <usuario>`
- `/help`
- `/quit`

## Guia para 2 PCs por switch

Si esta computadora sera el servidor y otra computadora sera el cliente,
usa la guia paso a paso en [docs/guia-switch-2pc.md](docs/guia-switch-2pc.md).
