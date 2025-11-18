#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _POSIX_C_SOURCE 200809L

typedef struct strucRegistro
{
    int estado;
    int bloqueado;
    char descripcion[100]; //si es 0 está libre, si es !=0 está bloqueado (contiene pid del proceso q lo bloquea)
} registro;

typedef struct msgbuf
{
    long tipo;
    char data[156];
} mensaje;


int buscarEspacio();
void decodificarMensaje();
void procesarPeticion();
void on_sigint(int s);
void liberarRegistros();

int  pid;
int  ins;
char descr[100];
int  cola;
int  fd;

mensaje  m;
registro r;

int main(int argc, char **argv) {
    //sigint
    signal(SIGINT, (void *)on_sigint);

    //creo cola de msj 0xa
    cola = msgget(0xa, IPC_CREAT | 0777);
    if (cola == -1) {
        printf("No se pudo abrir/crear la cola.\n");
        return -1;
    }

    if (access("registros", F_OK) == 0) {
        fd = open("registros", O_RDWR);
        if (fd >= 0) printf("Archivo 'registros' abierto en modo R/W.\n");
    } else {
        fd = open("registros", O_CREAT | O_TRUNC | O_RDWR | 0666);
        //inicializo los 1000 registros libres
        r.estado = 0;
        r.bloqueado = 0;
        strcpy(r.descripcion, "");
        for (int i = 0; i < 1000; i++) write(fd, &r, sizeof(r));
        printf("Archivo 'registros' creado e inicializado (1000 libres).\n");
    }

    // limpio posibles bloqueos antiguos 
    liberarRegistros();

    //atiendo la cola de msj
    memset(m.data, 0, 156);
    while (msgrcv(cola, &m, 156, 1, 0)) {
        printf("[SRV] Pedido: %s\n", m.data);

        decodificarMensaje();

        procesarPeticion();

        memset(m.data, 0, 156);
    }
}

void liberarRegistros() {
    registro r2;
    lseek(fd, 0, SEEK_SET);
    while(read(fd, &r2, sizeof(r)))
        if(r2.bloqueado != 0) {
            r2.bloqueado = 0;
            lseek(fd, -(sizeof(r2)), SEEK_CUR);
            write(fd, &r2, sizeof(r2));
        }
}

void procesarPeticion()
{
    //ignoro SIGINT hasta terminar de atender
    sigset_t set;
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set, NULL);

    if (ins == -1) {
        int num = buscarEspacio() + 1;
        if(num > 0) {
            lseek(fd, sizeof(r)*(num-1), SEEK_SET);
            read(fd, &r, sizeof(r));
            if(r.bloqueado == 0 || r.bloqueado == pid){
                r.estado = 1;
                strcpy(r.descripcion, descr);
                lseek(fd, sizeof(r)*(num-1), SEEK_SET);
                write(fd, &r, sizeof(registro));
                m.tipo = pid;
                sprintf(m.data, "1,%i,Registro creado correctamente", num);
                msgsnd(cola, &m, 156, 0);
            } else {
                m.tipo = pid;
                sprintf(m.data, "0,%i,Bloqueado por proceso %i", num, r.bloqueado);
                msgsnd(cola, &m, 156, 0);
            }
        } else {
            m.tipo = pid;
            strcpy(m.data, "0,No hay espacio disponible para nuevos registros");
            msgsnd(cola, &m, 156, 0);
        }
    } else {
        if (strncmp(descr, "leer", 4) == 0 || strncmp(descr, "borrar", 6) == 0 || strncmp(descr, "lock", 4) == 0 || strncmp(descr, "unlock", 6) == 0) {

            //read
            if (strncmp(descr, "leer", 4) == 0 && ins > 0 && ins <= 1000) {
                lseek(fd, sizeof(r) * (ins - 1), SEEK_SET);
                read(fd, &r, sizeof(r));
                m.tipo = pid;
                if (r.estado == 0 || r.estado == 2) {
                    sprintf(m.data, "0,%i,El registro solicitado no existe", ins);
                    msgsnd(cola, &m, 156, 0);
                } else {
                    if (r.bloqueado == 0 || r.bloqueado == pid){
                        sprintf(m.data, "1,%i,%s", ins, r.descripcion);
                        msgsnd(cola, &m, 156, 0);
                    } else {
                        sprintf(m.data, "0,%i,Registro bloqueado por %i", ins, r.bloqueado);
                        msgsnd(cola, &m, 156, 0);
                    }
                }
            }

            //borro
            if (strncmp(descr, "borrar", 6) == 0 && ins > 0 && ins <= 1000) {
                lseek(fd, sizeof(r) * (ins - 1), SEEK_SET);
                read(fd, &r, sizeof(r));
                m.tipo = pid;
                if (r.estado == 1) {
                    if (r.bloqueado == 0 || r.bloqueado == pid){
                        r.estado = 2;
                        lseek(fd, sizeof(r) * (ins - 1), SEEK_SET);
                        write(fd, &r, sizeof(r));
                        sprintf(m.data, "1,%i,Eliminado con éxito el registro %i", ins, ins);
                        msgsnd(cola, &m, 156, 0);
                    } else {
                        sprintf(m.data, "0,%i,Bloqueado por el proceso %i", ins, r.bloqueado);
                        msgsnd(cola, &m, 156, 0);
                    }
                } else {
                    sprintf(m.data, "0,%i,No existe el registro que intenta eliminar", ins);
                    msgsnd(cola, &m, 156, 0);
                }
            }

            //lock
            if (strncmp(descr, "lock", 4) == 0 && ins > 0 && ins <= 1000) {
                lseek(fd, sizeof(r)*(ins-1),SEEK_SET);
                read(fd, &r, sizeof(r));
                m.tipo = pid;
                if(r.bloqueado == 0) {
                    r.bloqueado = pid;
                    lseek(fd, sizeof(r)*(ins-1),SEEK_SET);
                    write(fd, &r, sizeof(r));
                    sprintf(m.data,"1,%i,Registro bloqueado correctamente",ins);
                    msgsnd(cola, &m, 156, 0);
                } else {
                    sprintf(m.data,"0,%i,Ya está bloqueado por %i",ins, r.bloqueado);
                    msgsnd(cola, &m, 156, 0);
                }
            }

            //unlock
            if (strncmp(descr, "unlock", 6) == 0 && ins > 0 && ins <= 1000) {
                lseek(fd, sizeof(r)*(ins-1),SEEK_SET);
                read(fd, &r, sizeof(r));
                m.tipo = pid;
                if(r.bloqueado == pid) {
                    r.bloqueado = 0;
                    lseek(fd, sizeof(r)*(ins-1),SEEK_SET);
                    write(fd, &r, sizeof(r));
                    sprintf(m.data,"1,%i,Registro desbloqueado",ins);
                    msgsnd(cola, &m, 156, 0);
                } else {
                    if(r.bloqueado == 0){
                        sprintf(m.data,"0,%i,El registro ya estaba libre",ins);
                        msgsnd(cola, &m, 156, 0);
                    } else {
                        sprintf(m.data,"0,%i,Sigue bloqueado por %i",ins, r.bloqueado);
                        msgsnd(cola, &m, 156, 0);
                    }
                }
            }
        } else {
            //modificar
            if (ins > 0 && ins <= 1000) {
                lseek(fd, sizeof(r) * (ins - 1), SEEK_SET);
                read(fd, &r, sizeof(r));
                m.tipo = pid;
                if(r.bloqueado == 0 || r.bloqueado == pid){
                    r.estado = 1;
                    strcpy(r.descripcion, descr);
                    lseek(fd, sizeof(r) * (ins - 1), SEEK_SET);
                    write(fd, &r, sizeof(r));
                    sprintf(m.data, "1,%i,Registro %i actualizado", ins, ins);
                    msgsnd(cola, &m, 156, 0);
                } else {
                    sprintf(m.data, "0,%i,No se puede modificar: bloqueado por %i", ins, r.bloqueado);
                    msgsnd(cola, &m, 156, 0);
                }
            }
        }
    }

    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

int buscarEspacio() { //read file hasta espacio vacio
    registro r2;
    lseek(fd, 0, SEEK_SET); //voy al principio del file
    int num = 0;
    while(read(fd, &r2, sizeof(r))) {
        if((r2.estado == 0 || r2.estado == 2) && r2.bloqueado == 0)
            return num;
        num++;
    }
    return -1;
}

void decodificarMensaje()
{
    printf("Procesando: %s\n", m.data);

    char *token;
    char *delimitadores = ",";

    token = strtok(m.data, delimitadores);
    if (token != NULL) {
        printf("Token pid: %s\n", token);
        pid = atoi(token);
    }

    token = strtok(NULL, delimitadores);
    if (token != NULL) {
        printf("Token ins: %s\n", token);
        ins = atoi(token);
    }

    token = strtok(NULL, delimitadores);
    if (token != NULL) {
        printf("Token descr: %s\n", token);
        strcpy(descr, token);
    }
    descr[strlen(token)] = '\0';
}

void on_sigint(int s)
{
    (void)s;
    msgctl(cola, IPC_RMID, 0);
    close(fd);
    raise(SIGKILL);
}
