#include <stdio.h>

#include "client.h"

void print_help(void) {
    printf("\n");
    printf("+--------------------------------------------------------+\n");
    printf("|                    COMANDOS DEL CHAT                  |\n");
    printf("+--------------------------------------------------------+\n");
    printf("|  /broadcast <msg>   - Enviar mensaje a todos          |\n");
    printf("|  /message <msg>     - Alias de /broadcast             |\n");
    printf("|  /msg <user> <msg>  - Mensaje privado a un usuario    |\n");
    printf("|  /status <estado>   - Cambiar status                  |\n");
    printf("|                      (ACTIVO, OCUPADO, INACTIVO)      |\n");
    printf("|  /list o /lis       - Listar usuarios conectados      |\n");
    printf("|  /info <user>       - Info de un usuario              |\n");
    printf("|  /help              - Mostrar esta ayuda              |\n");
    printf("|  /quit              - Salir del chat                  |\n");
    printf("+--------------------------------------------------------+\n");
    printf("\n");
}
