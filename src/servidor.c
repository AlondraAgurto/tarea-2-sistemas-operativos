
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include "servidor.h"

/* Control de mesas disponibles */
static sem_t mesas_disponibles;

/* Protege la asignacion de numeros de mesa */
static pthread_mutex_t mutex_mesas = PTHREAD_MUTEX_INITIALIZER;

static int *mesas_ocupadas;
static int total_mesas;

typedef struct {
    int id_partida;
    int jugador_puma;
    int jugador_perros;
} DatosPartida;

/* Simula temporalmente una partida */
static void simular_juego(void) {
    struct timespec tiempo = {2, 0};
    nanosleep(&tiempo, NULL);
}

/* Busca y reserva una mesa libre */
static int reservar_mesa(void) {
    int mesa = -1;

    pthread_mutex_lock(&mutex_mesas);

    for (int i = 0; i < total_mesas; i++) {
        if (mesas_ocupadas[i] == 0) {
            mesas_ocupadas[i] = 1;
            mesa = i;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_mesas);

    return mesa;
}

/* Libera una mesa al terminar la partida */
static void liberar_mesa(int mesa) {
    pthread_mutex_lock(&mutex_mesas);
    mesas_ocupadas[mesa] = 0;
    pthread_mutex_unlock(&mutex_mesas);

    sem_post(&mesas_disponibles);
}

/* Funcion ejecutada por cada hilo de partida */
static void *ejecutar_mesa(void *arg) {
    DatosPartida *datos = (DatosPartida *)arg;

    /* Esperar pasivamente un cupo */
    while (sem_wait(&mesas_disponibles) == -1) {
        /* Manejo de interrupciones se ampliara luego */
    }

    int mesa = reservar_mesa();

    printf("Partida %d: Puma %d vs Perros %d "
           "-> Mesa %d\n",
           datos->id_partida,
           datos->jugador_puma,
           datos->jugador_perros,
           mesa + 1);

    simular_juego();

    printf("Partida %d finalizada en mesa %d\n",
           datos->id_partida, mesa + 1);

    liberar_mesa(mesa);

    return NULL;
}

void iniciar_servidor(int n, int m) {
    int cantidad_partidas = n / 2;
    total_mesas = m;

    mesas_ocupadas = calloc(m, sizeof(int));

    if (mesas_ocupadas == NULL) {
        perror("Error al reservar memoria");
        return;
    }

    if (sem_init(&mesas_disponibles, 0, m) != 0) {
        perror("Error al crear semaforo");
        free(mesas_ocupadas);
        return;
    }

    pthread_t *hilos = calloc(
        cantidad_partidas, sizeof(pthread_t));

    DatosPartida *datos = calloc(
        cantidad_partidas, sizeof(DatosPartida));

    if (hilos == NULL || datos == NULL) {
        perror("Error al reservar memoria");
        free(hilos);
        free(datos);
        sem_destroy(&mesas_disponibles);
        free(mesas_ocupadas);
        return;
    }

    printf("\nServidor iniciado.\n");
    printf("Partidas a procesar: %d\n", cantidad_partidas);
    printf("Mesas disponibles: %d\n\n", m);

    int creados = 0;

    for (int i = 0; i < cantidad_partidas; i++) {
        datos[i].id_partida = i + 1;
        datos[i].jugador_puma = 2 * i + 1;
        datos[i].jugador_perros = 2 * i + 2;

        if (pthread_create(&hilos[i], NULL,
                           ejecutar_mesa, &datos[i]) != 0) {
            fprintf(stderr, "Error al crear hilo %d\n", i + 1);
            break;
        }

        creados++;
    }

    /* Esperar a que terminen las partidas */
    for (int i = 0; i < creados; i++) {
        pthread_join(hilos[i], NULL);
    }

    sem_destroy(&mesas_disponibles);
    free(hilos);
    free(datos);
    free(mesas_ocupadas);

    printf("\nTodas las partidas han terminado.\n");
}
