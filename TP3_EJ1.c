
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define MAX 512

// var global para salida
volatile sig_atomic_t salir = 0;

// handler sigint
void handler(int sig) {
    (void)sig;
    salir = 1;
}


int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <fifo_escritura> <fifo_lectura>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *fifo_esc = argv[1];
    char *fifo_lec = argv[2];
    char mensaje[MAX];
    char recibido[MAX];
    int fd_esc, fd_lec;
    ssize_t nbytes;

    signal(SIGINT, handler);

    printf("Espero apertura fifos\n");

    //fifo read
    fd_lec = open(fifo_lec, O_RDONLY | O_NONBLOCK);
    if (fd_lec == -1) {
        perror("Error al abrir FIFO de lectura");
        return EXIT_FAILURE;
    }

    //fifo write
    fd_esc = open(fifo_esc, O_WRONLY);
    if (fd_esc == -1) {
        perror("Error al abrir FIFO de escritura");
        close(fd_lec);
        return EXIT_FAILURE;
    }

    //bloq fifo read
    int flags = fcntl(fd_lec, F_GETFL, 0);
    fcntl(fd_lec, F_SETFL, flags & ~O_NONBLOCK);

    printf("Chat iniciado\n");
    printf("Escribi mensajes y presiona ENTER, Para salir escribi: chau\n");

    while (!salir) {
        fd_set conj_lectura;
        FD_ZERO(&conj_lectura);
        FD_SET(STDIN_FILENO, &conj_lectura); 
        FD_SET(fd_lec, &conj_lectura);       

        int maxfd = (fd_lec > STDIN_FILENO ? fd_lec : STDIN_FILENO);
        int rv = select(maxfd + 1, &conj_lectura, NULL, NULL, NULL);

        if (rv == -1) {
            perror("select");
            break;
        }

        
        if (FD_ISSET(STDIN_FILENO, &conj_lectura)) {
            if (fgets(mensaje, sizeof(mensaje), stdin) == NULL)
                break;

            //borro salto de linea
            size_t len = strlen(mensaje);
            if (len > 0 && mensaje[len - 1] == '\n')
                mensaje[len - 1] = '\0';

            //envio msj
            write(fd_esc, mensaje, strlen(mensaje) + 1);

            if (strcmp(mensaje, "chau") == 0) {
                printf("Fin del chat local.\n");
                break;
            }
        }

        //mostrar msj recibido
        if (FD_ISSET(fd_lec, &conj_lectura)) {
            nbytes = read(fd_lec, recibido, sizeof(recibido));

            if (nbytes == 0) {
                printf("El otro proceso cerró el chat.\n");
                break;
            }

            if (nbytes > 0) {
                recibido[nbytes] = '\0';
                if (strcmp(recibido, "chau") == 0) {
                    printf("El otro proceso dijo chau.\n");
                    break;
                }
                printf("%s\n", recibido);
            }
        }
    }

    close(fd_esc);
    close(fd_lec);

    printf("Chat finalizado\n");
    return EXIT_SUCCESS;
}
