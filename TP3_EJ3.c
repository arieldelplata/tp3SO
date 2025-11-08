#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define MAXMSG 512

static volatile sig_atomic_t stop = 0;
static void on_sigint(int s){ (void)s; stop = 1; }

struct Chat {
    int turno;           // 1 o 2 puede escribir
    int listo1;          // indica si proc 1 envio mensaje
    int listo2;          // indica si proc 2 envio mensaje
    char msg1[MAXMSG];   // ultimo mensaje de 1
    char msg2[MAXMSG];   // ultimo mensaje de 2
};

int main(int argc, char *argv[]){
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <path_ftok> <proj_id> <mi_id>\n"
                        "Ejemplo:\n  ./tp3ej3 /tmp 65 1\n  ./tp3ej3 /tmp 65 2\n",
                        argv[0]);
        return 1;
    }

    const char *path = argv[1];
    int proj_id = atoi(argv[2]);
    int me = atoi(argv[3]);
    int other = (me == 1 ? 2 : 1);

    key_t key = ftok(path, proj_id);
    if (key == (key_t)-1) { perror("ftok"); return 1; }

    int shm_id = shmget(key, sizeof(struct Chat), IPC_CREAT | IPC_EXCL | 0666);
    int owner = 0;
    if (shm_id == -1) {
        shm_id = shmget(key, sizeof(struct Chat), 0666);
        if (shm_id == -1) { perror("shmget"); return 1; }
    } else {
        owner = 1;
    }

    struct Chat *chat = (struct Chat*) shmat(shm_id, NULL, 0);
    if (chat == (void*)-1) { perror("shmat"); return 1; }

    signal(SIGINT, on_sigint);

    if (owner) {
        chat->turno = 1;
        chat->listo1 = chat->listo2 = 0;
        chat->msg1[0] = chat->msg2[0] = '\0';
    }

    printf("Chat SHM listo. Soy el proceso %d. Para salir: chau\n", me);

    while (!stop) {
        // leer msj de la otra terminal 
        if (me == 1 && chat->listo2) {
            printf("Peer: %s\n", chat->msg2);
            fflush(stdout);
            if (strcmp(chat->msg2, "chau") == 0) break;
            chat->listo2 = 0;
            chat->turno = 1;
        } else if (me == 2 && chat->listo1) {
            printf("Peer: %s\n", chat->msg1);
            fflush(stdout);
            if (strcmp(chat->msg1, "chau") == 0) break;
            chat->listo1 = 0;
            chat->turno = 2;
        }

        // si es mi turno leo entrada y escribo
        if (chat->turno == me) {
            char linea[MAXMSG];
            printf("Yo: ");
            fflush(stdout);
            if (!fgets(linea, sizeof(linea), stdin)) break;
            size_t len = strlen(linea);
            if (len && linea[len-1] == '\n') linea[len-1] = '\0';

            if (me == 1) {
                strncpy(chat->msg1, linea, MAXMSG);
                chat->msg1[MAXMSG-1] = '\0';
                chat->listo1 = 1;
                chat->turno = 2;
            } else {
                strncpy(chat->msg2, linea, MAXMSG);
                chat->msg2[MAXMSG-1] = '\0';
                chat->listo2 = 1;
                chat->turno = 1;
            }
            if (strcmp(linea, "chau") == 0) break;
        }
        usleep(100000); // 0.1s para evitar ocupar CPU
    }

    printf("Finalizando...\n");
    shmdt(chat);

    if (owner) {
        shmctl(shm_id, IPC_RMID, NULL);
        printf("Segmento de memoria eliminado.\n");
    }
    return 0;
}