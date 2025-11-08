#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define MAXMSG 512

static volatile sig_atomic_t stop = 0;
static void on_sigint(int s){ (void)s; stop = 1; }

struct msgbufx {
    long mtype;           // >=1
    char mtext[MAXMSG];   
};

static long parse_long(const char *s){
    char *end = NULL;
    long v = strtol(s, &end, 0);   //como es 0,  acepta tipos 123 o 0x41 
    if (end == s || *end != '\0') {
        fprintf(stderr, "Valor inválido: %s\n", s);
        exit(1);
    }
    return v;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr,
            "Uso: %s <path_ftok> <proj_id> <mi_tipo> <su_tipo>\n"
            "Ej.: %s /tmp 65 1 2\n", argv[0], argv[0]);
        return 1;
    }

    const char *path = argv[1];
    long proj_id = parse_long(argv[2]);
    long my_type = parse_long(argv[3]);
    long his_type = parse_long(argv[4]);

    if (proj_id < 0 || proj_id > 255 || my_type < 1 || his_type < 1) {
        fprintf(stderr, "proj_id en 0..255, tipos >= 1\n");
        return 1;
    }

    key_t key = ftok(path, (int)proj_id);
    if (key == (key_t)-1) { perror("ftok"); return 1; }

    //si no existe la cola, la creo.
    int i_am_owner = 0;
    int msqid = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    if (msqid == -1) {
        msqid = msgget(key, 0666);
        if (msqid == -1) { perror("msgget"); return 1; }
        i_am_owner = 0;
    } else {
        i_am_owner = 1;
    }

    signal(SIGINT, on_sigint);
    printf("KEY=0x%lx, msqid=%d, envio tipo %ld, recibo tipo %ld.\n",
           (unsigned long)key, msqid, my_type, his_type);
    printf("Para salir escriba: chau\n");

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        //hijo (receptor)
        struct msgbufx in;
        while (!stop) {
            ssize_t n = msgrcv(msqid, &in, sizeof(in.mtext), his_type, 0);
            if (n < 0) { perror("msgrcv"); break; }
            in.mtext[sizeof(in.mtext)-1] = '\0';
            if (strcmp(in.mtext, "chau") == 0) {
                printf("\nPeer dijo 'chau'.\n");
                break;
            }
            printf("Peer: %s\n", in.mtext);
            fflush(stdout);
        }
        _exit(0);
    }

    //padre (emite msj)
    struct msgbufx out;
    char line[MAXMSG];

    while (!stop) {
        if (!fgets(line, sizeof(line), stdin)) break;
        size_t len = strlen(line);
        if (len && line[len-1] == '\n') line[len-1] = '\0';

        out.mtype = my_type;
        strncpy(out.mtext, line, sizeof(out.mtext));
        out.mtext[sizeof(out.mtext)-1] = '\0';

        if (msgsnd(msqid, &out, strlen(out.mtext)+1, 0) < 0) {
            perror("msgsnd");
            break;
        }
        if (strcmp(out.mtext, "chau") == 0) {
            printf("Enviaste 'chau'.\n");
            break;
        }
    }

    //terminar hijo o esperar
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);

    //si cree la cola, la puedo eliminar, y la elimino
    if (i_am_owner) {
        if (msgctl(msqid, IPC_RMID, NULL) == -1) perror("msgctl(IPC_RMID)");
        else puts("Cola eliminada.");
    }
    return 0;
}