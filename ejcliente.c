#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

typedef struct strucRegistro {
    int estado;
    int bloqueado;
    char descripcion[100];
} registro;

typedef struct msgbuf {
    long tipo;
    char data[156];
} mensaje;


void decodificarMensaje(int* pid, int* ins, char** descr, mensaje* m);

int cola;
int pid;

int main(int argc, char **argv) {
    pid = getpid();

    //creo cola de msj
    cola = msgget(0xa, IPC_CREAT | 0777);
    if (cola == -1) {
        printf("No se pudo crear/abrir la cola de mensajes\n");
        return -1;
    }

    mensaje m;
    m.tipo = 1;
    char texto[130];

    //loop while para atender la cola
    while(1) {
        memset(m.data, 0, 156);

        //cliente envia con su pid (tipo 1)
        printf("[PID %i] > ", pid);
        fgets(texto, 130, stdin);
        texto[strlen(texto)-1] = '\0';

        sprintf(m.data, "%i,%s", pid, texto);
        printf("TX -> %s\n", m.data);
        m.tipo = 1;
        msgsnd(cola, &m, 156, 0);

        //esperando respuesta del server
        printf("Esperando al servidor...\n");
        msgrcv(cola, &m, 156, pid, 0);
        printf("RX <- %s\n", m.data);
    }
}

void decodificarMensaje(int* pid, int* ins, char** descr, mensaje* m) {
    printf("procesando (cliente): %s\n", (*m).data);
}
