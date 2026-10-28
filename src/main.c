#include <stdio.h>
#include <stdlib.h>
#include "servidor.h"

int main(int argc, char *argv[]) {

    if (argc < 3 || argc > 4) {
        fprintf(stderr,
                "Uso: %s N M [config.txt]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    char *fin;
    long n = strtol(argv[1], &fin, 10);

    if (*fin != '\0' || n <= 0) {
        fprintf(stderr, "Error: N debe ser positivo.\n");
        return EXIT_FAILURE;
    }

    long m = strtol(argv[2], &fin, 10);

    if (*fin != '\0' || m <= 0) {
        fprintf(stderr, "Error: M debe ser positivo.\n");
        return EXIT_FAILURE;
    }

    if (n % 2 != 0) {
        fprintf(stderr,
                "Error: N debe ser par para formar parejas.\n");
        return EXIT_FAILURE;
    }

    printf("=== TORNEO KOMIKAN ===\n");
    printf("Jugadores: %ld\n", n);
    printf("Maximo de mesas: %ld\n", m);

    if (argc == 4) {
        printf("Configuracion: %s\n", argv[3]);
    }

    iniciar_servidor((int)n, (int)m);

    return EXIT_SUCCESS;
}