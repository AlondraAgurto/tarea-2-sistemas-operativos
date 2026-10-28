
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>

#include "servidor.h"

#define DURACION_PARTIDA_MS 2000
#define INTERVALO_MS 100

/* Control de interrupciones */
static volatile sig_atomic_t sigint_recibida = 0;
static atomic_bool error_servidor = ATOMIC_VAR_INIT(0);

/* Control de mesas */
static sem_t mesas_disponibles;
static pthread_mutex_t mutex_mesas = PTHREAD_MUTEX_INITIALIZER;

static int *mesas_ocupadas = NULL;
static int total_mesas = 0;

/* Control de emparejamiento */
static pthread_mutex_t mutex_parejas = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_parejas = PTHREAD_COND_INITIALIZER;

static int jugadores_llegados = 0;

typedef struct {
    int id;
    int jugador_puma;
    int jugador_perros;
    int lista;
} Pareja;

typedef struct {
    int id;
    Pareja *parejas;
    pthread_t *jueces;
    int *juez_creado;
} Cliente;

/* Consulta si corresponde detener el servidor */
static int servidor_detenido(void) {
    return sigint_recibida || atomic_load(&error_servidor);
}

/* Manejador de Ctrl+C */
static void manejar_sigint(int signo) {
    (void)signo;
    sigint_recibida = 1;
}

/* Dormir una cantidad de milisegundos */
static void dormir_ms(long ms) {
    struct timespec tiempo = {
        .tv_sec = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000L
    };

    while (nanosleep(&tiempo, &tiempo) == -1 &&
           errno == EINTR && !servidor_detenido()) {
    }
}

/* Obtiene un limite de espera de 100 ms */
static int tiempo_limite(struct timespec *limite) {
    if (clock_gettime(CLOCK_REALTIME, limite) == -1) {
        return -1;
    }

    limite->tv_nsec += 100000000L;

    if (limite->tv_nsec >= 1000000000L) {
        limite->tv_sec++;
        limite->tv_nsec -= 1000000000L;
    }

    return 0;
}

/* Esperar pasivamente una mesa disponible */
static int esperar_mesa(void) {
    while (!servidor_detenido()) {
        struct timespec limite;

        if (tiempo_limite(&limite) == -1) {
            return -1;
        }

        if (sem_timedwait(&mesas_disponibles, &limite) == 0) {
            return 0;
        }

        if (errno != EINTR && errno != ETIMEDOUT) {
            return -1;
        }
    }

    return -1;
}

/* Reservar una mesa fisica */
static int reservar_mesa(void) {
    int mesa = -1;

    pthread_mutex_lock(&mutex_mesas);

    for (int i = 0; i < total_mesas; i++) {
        if (!mesas_ocupadas[i]) {
            mesas_ocupadas[i] = 1;
            mesa = i;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_mesas);

    return mesa;
}

/* Liberar mesa y devolver el permiso */
static void liberar_mesa(int mesa) {
    pthread_mutex_lock(&mutex_mesas);

    mesas_ocupadas[mesa] = 0;

    pthread_mutex_unlock(&mutex_mesas);

    sem_post(&mesas_disponibles);
}

/* Hilo juez: por ahora simula la partida */
static void *ejecutar_juez(void *arg) {
    Pareja *pareja = (Pareja *)arg;

    if (esperar_mesa() != 0) {
        printf("Partida %d cancelada esperando mesa.\n",
               pareja->id);
        return NULL;
    }

    if (servidor_detenido()) {
        sem_post(&mesas_disponibles);
        return NULL;
    }

    int mesa = reservar_mesa();

    if (mesa == -1) {
        fprintf(stderr, "No se pudo reservar una mesa.\n");
        atomic_store(&error_servidor, 1);
        sem_post(&mesas_disponibles);
        return NULL;
    }

    printf("Partida %d: Puma %d vs Perros %d "
           "-> Mesa %d\n",
           pareja->id,
           pareja->jugador_puma,
           pareja->jugador_perros,
           mesa + 1);

    /* Simulacion temporal: reemplazar por juego real */
    int transcurrido = 0;

    while (transcurrido < DURACION_PARTIDA_MS &&
           !servidor_detenido()) {
        dormir_ms(INTERVALO_MS);
        transcurrido += INTERVALO_MS;
    }

    if (servidor_detenido()) {
        printf("Partida %d interrumpida.\n", pareja->id);
    } else {
        printf("Partida %d finalizada.\n", pareja->id);
    }

    liberar_mesa(mesa);

    return NULL;
}

/* Hilo que representa la llegada de un jugador */
static void *ejecutar_cliente(void *arg) {
    Cliente *cliente = (Cliente *)arg;

    /* Simular distintos tiempos de llegada */
    dormir_ms(20 + (cliente->id * 37L) % 180);

    if (servidor_detenido()) {
        return NULL;
    }

    pthread_mutex_lock(&mutex_parejas);

    if (servidor_detenido()) {
        pthread_mutex_unlock(&mutex_parejas);
        return NULL;
    }

    int posicion = jugadores_llegados++;
    int indice_pareja = posicion / 2;

    Pareja *pareja = &cliente->parejas[indice_pareja];

    printf("Jugador %d ha llegado al servidor.\n",
           cliente->id);

    if (posicion % 2 == 0) {
        /* Primer jugador: se convierte en Puma */
        pareja->id = indice_pareja + 1;
        pareja->jugador_puma = cliente->id;

        printf("Jugador %d espera pareja.\n",
               cliente->id);

        /* Espera pasiva mediante variable de condicion */
        while (!pareja->lista && !servidor_detenido()) {
            struct timespec limite;

            if (tiempo_limite(&limite) == -1) {
                atomic_store(&error_servidor, 1);
                break;
            }

            int resultado = pthread_cond_timedwait(
                &cond_parejas, &mutex_parejas, &limite);

            if (resultado != 0 && resultado != ETIMEDOUT) {
                atomic_store(&error_servidor, 1);
                break;
            }
        }

        pthread_mutex_unlock(&mutex_parejas);
        return NULL;
    }

    /* Segundo jugador: se convierte en Perros */
    pareja->jugador_perros = cliente->id;
    pareja->lista = 1;

    printf("Pareja %d formada: Puma %d vs Perros %d\n",
           pareja->id,
           pareja->jugador_puma,
           pareja->jugador_perros);

    pthread_cond_broadcast(&cond_parejas);
    pthread_mutex_unlock(&mutex_parejas);

    if (servidor_detenido()) {
        return NULL;
    }

    /* Crear juez independiente para esta pareja */
    int resultado = pthread_create(
        &cliente->jueces[indice_pareja],
        NULL,
        ejecutar_juez,
        pareja
    );

    if (resultado != 0) {
        fprintf(stderr, "Error al crear juez: %d\n",
                resultado);
        atomic_store(&error_servidor, 1);
        return NULL;
    }

    cliente->juez_creado[indice_pareja] = 1;

    return NULL;
}

/* Funcion principal del servidor */
void iniciar_servidor(int n, int m) {
    sigint_recibida = 0;
    atomic_store(&error_servidor, 0);
    jugadores_llegados = 0;
    total_mesas = m;

    int cantidad_partidas = n / 2;

    struct sigaction accion = {0};
    struct sigaction anterior;

    accion.sa_handler = manejar_sigint;
    sigemptyset(&accion.sa_mask);

    if (sigaction(SIGINT, &accion, &anterior) == -1) {
        perror("Error al registrar SIGINT");
        return;
    }

    mesas_ocupadas = calloc(m, sizeof(int));
    pthread_t *hilos_clientes = calloc(n, sizeof(pthread_t));
    pthread_t *jueces = calloc(cantidad_partidas,
                              sizeof(pthread_t));
    Cliente *clientes = calloc(n, sizeof(Cliente));
    Pareja *parejas = calloc(cantidad_partidas,
                            sizeof(Pareja));
    int *juez_creado = calloc(cantidad_partidas, sizeof(int));

    if (!mesas_ocupadas || !hilos_clientes ||
        !jueces || !clientes || !parejas || !juez_creado) {
        perror("Error al reservar memoria");
        goto limpiar_memoria;
    }

    if (sem_init(&mesas_disponibles, 0, m) == -1) {
        perror("Error al crear semaforo");
        goto limpiar_memoria;
    }

    printf("\n=== SERVIDOR DE EMPAREJAMIENTO ===\n");
    printf("Jugadores esperados: %d\n", n);
    printf("Mesas disponibles: %d\n\n", m);

    int creados = 0;

    /* Crear los N clientes concurrentes */
    for (int i = 0; i < n; i++) {
        if (servidor_detenido()) {
            break;
        }

        clientes[i].id = i + 1;
        clientes[i].parejas = parejas;
        clientes[i].jueces = jueces;
        clientes[i].juez_creado = juez_creado;

        int resultado = pthread_create(
            &hilos_clientes[i],
            NULL,
            ejecutar_cliente,
            &clientes[i]
        );

        if (resultado != 0) {
            fprintf(stderr,
                    "Error al crear cliente: %d\n",
                    resultado);
            atomic_store(&error_servidor, 1);
            break;
        }

        creados++;
    }

    /* Esperar a todos los clientes */
    for (int i = 0; i < creados; i++) {
        pthread_join(hilos_clientes[i], NULL);
    }

    /* Esperar a los jueces que fueron creados */
    for (int i = 0; i < cantidad_partidas; i++) {
        if (juez_creado[i]) {
            pthread_join(jueces[i], NULL);
        }
    }

    if (servidor_detenido()) {
        printf("\nServidor detenido ordenadamente.\n");
    } else {
        printf("\nTorneo finalizado correctamente.\n");
    }

    sem_destroy(&mesas_disponibles);

limpiar_memoria:
    free(mesas_ocupadas);
    free(hilos_clientes);
    free(jueces);
    free(clientes);
    free(parejas);
    free(juez_creado);

    mesas_ocupadas = NULL;

    sigaction(SIGINT, &anterior, NULL);

    printf("Recursos del servidor liberados.\n");
}
