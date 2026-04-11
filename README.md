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

- `server.c`: punto de entrada del servidor.
- `server.h`: tipos, constantes y prototipos compartidos del servidor.
- `server_utils.c`: estado global y utilidades del servidor.
- `server_threads.c`: hilos de inactividad y manejo de clientes.
- `client.c`: punto de entrada del cliente.
- `client.h`: constantes, estado compartido y prototipos del cliente.
- `client_receive.c`: hilo receptor y parseo de respuestas del servidor.
- `client_ui.c`: ayuda y salida de interfaz en consola.

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
gcc -Wall -Wextra -pthread -g -o chat_server server.c server_utils.c server_threads.c
gcc -Wall -Wextra -pthread -g -o chat_client client.c client_receive.c client_ui.c
```

## Ejecucion

1. Levantar el servidor:

```bash
./chat_server <puerto>
```

2. Conectar clientes (en otras terminales):

```bash
./chat_client <usuario> <ip_servidor> <puerto>
```

Ejemplo local:

```bash
./chat_server 8080
./chat_client Ana 127.0.0.1 8080
```

## Comandos del cliente

- `/broadcast <mensaje>`
- `/msg <usuario> <mensaje>`
- `/status <ACTIVO|OCUPADO|INACTIVO>`
- `/list`
- `/info <usuario>`
- `/help`
- `/quit`