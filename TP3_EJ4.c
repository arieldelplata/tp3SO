#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define ITER 5   //cant de iteraciones

static volatile sig_atomic_t stop = 0;
static void on_sigint(int s){ (void)s; stop = 1; }

void P(int semid){
    struct sembuf op = {0, -1, 0};
    semop(semid, &op, 1);
}
void V(int semid){
    struct sembuf op = {0, +1, 0};
    semop(semid, &op, 1);
}

int main(int argc, char *argv[]){
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <path_ftok> <proj_id> <mi_id>\n"
                        "Ejemplo:\n  ./tp3ej4 /tmp 65 1\n  ./tp3ej4 /tmp 65 2\n",
                        argv[0]);
        return 1;
    }

    const char *path = argv[1];
    int proj_id = atoi(argv[2]);
    int me = atoi(argv[3]);

    key_t key = ftok(path, proj_id);
    if (key == (key_t)-1) { perror("ftok"); return 1; }

    int semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0666);
    int owner = 0;
    if (semid == -1) {
        semid = semget(key, 1, 0666);
        if (semid == -1) { perror("semget"); return 1; }
    } else {
        owner = 1;
        semctl(semid, 0, SETVAL, 1);  //inicializo sem en 1
    }

    signal(SIGINT, on_sigint);
    printf("Semáforo listo. Soy proceso %d.\n", me);
    printf("Iterare %d veces\n", ITER);

    for (int i = 0; i < ITER && !stop; i++) {
        P(semid);  //iter

        printf("[Proceso %d]Entrando a iteracion(%d)\n", me, i+1);
        fflush(stdout);
        sleep(1);  
        printf("[Proceso %d]Saliendo de iteracion (%d)\n", me, i+1);
        fflush(stdout);

        V(semid);  //fin iter

        sleep(1);  //fuera de iter
    }

    if (owner) {
        semctl(semid, 0, IPC_RMID, 0);
        printf("Semáforo eliminado (es owner).\n");
    }

    printf("Proceso %d finalizado.\n", me);
    return 0;
}