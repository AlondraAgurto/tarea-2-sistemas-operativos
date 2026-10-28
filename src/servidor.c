
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#include "servidor.h"

#define DURACION_SIMULADA_MS 2000
#define INTERVALO_REVISION_MS 100

/* Indicador de interrupcion */
static volatile sig_atomic_t detener_servidor = 0;

/* Recursos compartidos */
static sem_t mesas_disponibles;
static pthread_mutex_t mutex_mesas = PTHREAD_MUTEX_INITIALIZER;

static int *mesas_ocupadas = NULL;
static int total_mesas = 0;

typedef struct {
    int id_partida;
    int jugador_puma;
    int jugador_perros;
} DatosPartida;

/* Se ejecuta cuando el usuario presiona Ctrl+C */
static void manejar_sigint(int signo) {
    (void)signo;
    detener_servidor = 1;
}

/* Pausa de milisegundos */
static void dormir_ms(long ms) {
    struct timespec tiempo;

    tiempo.tv_sec = ms / 1000;
    tiempo.tv_nsec = (ms % 1000) * 1000000L;

    while (nanosleep(&tiempo, &tiempo) == -1 &&
           errno == EINTR) {
        if (detener_servidor) {
            return;
        }
    }
}

/* Espera un cupo, revisando periodicamente SIGINT */
static int esperar_mesa(void) {
    while (!detener_servidor) {
        struct timespec limite;

        if (clock_gettime(CLOCK_REALTIME, &limite) != 0) {
            return -1;
        }

        limite.tv_nsec += 100000000L;

        if (limite.tv_nsec >= 1000000000L) {
            limite.tv_sec++;
            limite.tv_nsec -= 1000000000L;
        }

        if (sem_timedwait(&mesas_disponibles, &limite) == 0) {
            return 0;
        }

        if (errno != ETIMEDOUT && errno != EINTR) {
            return -1;
        }
    }

    return -1;
}

/* Reserva una mesa disponible */
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

/* Libera la mesa y devuelve el permiso */
static void liberar_mesa(int mesa) {
    pthread_mutex_lock(&mutex_mesas);

    mesas_ocupadas[mesa] = 0;

    pthread_mutex_unlock(&mutex_mesas);

    sem_post(&mesas_disponibles);
}

/* Simulacion temporal del juego */
static void simular_juego(void) {
    int transcurrido = 0;

    while (transcurrido < DURACION_SIMULADA_MS &&
           !detener_servidor) {

        dormir_ms(INTERVALO_REVISION_MS);
        transcurrido += INTERVALO_REVISION_MS;
    }
}

/* Hilo responsable de cada partida */
static void *ejecutar_mesa(void *arg) {
    DatosPartida *datos = (DatosPartida *)arg;

    if (esperar_mesa() != 0) {
        printf("Partida %d cancelada antes de ingresar.\n",
               datos->id_partida);
        return NULL;
    }

    /* Si llego SIGINT, devolver el permiso */
    if (detener_servidor) {
        sem_post(&mesas_disponibles);
        return NULL;
    }

    int mesa = reservar_mesa();

    if (mesa == -1) {
        fprintf(stderr, "Error al reservar una mesa.\n");
        sem_post(&mesas_disponibles);
        return NULL;
    }

    printf("Partida %d: Puma %d vs Perros %d "
           "-> Mesa %d\n",
           datos->id_partida,
           datos->jugador_puma,
           datos->jugador_perros,
           mesa + 1);

    simular_juego();

    if (detener_servidor) {
        printf("Partida %d interrumpida en mesa %d\n",
               datos->id_partida, mesa + 1);
    } else {
        printf("Partida %d finalizada en mesa %d\n",
               datos->id_partida, mesa + 1);
    }

    liberar_mesa(mesa);

    return NULL;
}

/* Funcion principal del servidor */
void iniciar_servidor(int n, int m) {
    detener_servidor = 0;

    int cantidad_partidas = n / 2;
    total_mesas = m;

    struct sigaction accion;
    struct sigaction accion_anterior;

    accion.sa_handler = manejar_sigint;
    sigemptyset(&accion.sa_mask);
    accion.sa_flags = 0;

    if (sigaction(SIGINT, &accion, &accion_anterior) == -1) {
        perror("Error al registrar SIGINT");
        return;
    }

    mesas_ocupadas = calloc(m, sizeof(int));

    if (mesas_ocupadas == NULL) {
        perror("Error al reservar memoria");
        goto restaurar_senal;
    }

    if (sem_init(&mesas_disponibles, 0, m) != 0) {
        perror("Error al crear semaforo");
        goto liberar_mesas;
    }

    pthread_t *hilos = calloc(
        cantidad_partidas, sizeof(pthread_t));

    DatosPartida *datos = calloc(
        cantidad_partidas, sizeof(DatosPartida));

    if (hilos == NULL || datos == NULL) {
        perror("Error al reservar memoria");
        free(hilos);
        free(datos);
        goto destruir_semaforo;
    }

    printf("\nServidor iniciado.\n");
    printf("Partidas a procesar: %d\n", cantidad_partidas);
    printf("Mesas disponibles: %d\n\n", m);
    fflush(stdout);

    int creados = 0;

    for (int i = 0; i < cantidad_partidas; i++) {
        if (detener_servidor) {
            break;
        }

        datos[i].id_partida = i + 1;
        datos[i].jugador_puma = 2 * i + 1;
        datos[i].jugador_perros = 2 * i + 2;

        int resultado = pthread_create(
            &hilos[i],
            NULL,
            ejecutar_mesa,
            &datos[i]
        );

        if (resultado != 0) {
            fprintf(stderr,
                    "Error al crear hilo %d: %d\n",
                    i + 1, resultado);
            detener_servidor = 1;
            break;
        }

        creados++;
    }

    /* Esperar a todos los hilos creados */
    for (int i = 0; i < creados; i++) {
        pthread_join(hilos[i], NULL);
    }

    if (detener_servidor) {
        printf("\nServidor detenido ordenadamente.\n");
    } else {
        printf("\nTodas las partidas han terminado.\n");
    }

    free(hilos);
    free(datos);

destruir_semaforo:
    sem_destroy(&mesas_disponibles);

liberar_mesas:
    free(mesas_ocupadas);
    mesas_ocupadas = NULL;

restaurar_senal:
    sigaction(SIGINT, &accion_anterior, NULL);

    printf("Recursos del servidor liberados.\n");
}
