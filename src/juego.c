#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "juego.h"

typedef struct { // Estructura para pasar argumentos a los hilos de las fichas
    PartidaKomikan *partida; // Puntero a la partida de Komikan
    int tipo_ficha; // Tipo de ficha (PUMA o PERRO)
    Posicion pos_actual; // Posición actual de la ficha
    int id_ficha; // 0=Puma, 1..12=Perros
} ArgHiloFicha;

void inicializar_juego(PartidaKomikan *partida) { // Inicializar el juego Komikan
    // Inicializar el mutex del tablero
    pthread_mutex_init(&partida->mutex_tablero, NULL);
    
    partida->perros_capturados = 0;
    partida->juego_terminado = 0;
    partida->ganador = 0;
    partida->movimientos = 0;
    partida->cancelar = NULL;
    for (int i = 0; i < 13; i++) partida->vivo[i] = 1;

    // Limpiar todo el tablero dejándolo en VACIO (0)
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 5; j++) {
            partida->tablero[i][j] = VACIO;
            partida->identidad[i][j] = -1;
        }
    }

    // Colocamos al PUMA en una posición inicial (por ejemplo, en el centro del tablero)
    partida->tablero[4][2] = PUMA;
    partida->identidad[4][2] = 0;

    // Doce perros en casillas distintas, sin superponer al Puma.
    // La fila 0 representa un acceso al trono (solo columna 2).
    int colocados = 0;
    for (int i = 1; i <= 3 && colocados < 12; i++) {
        for (int j = 0; j < 5 && colocados < 12; j++) {
            partida->tablero[i][j] = PERRO;
            partida->identidad[i][j] = colocados + 1;
            colocados++;
        }
    }
}
// Función para imprimir el estado actual del tablero
void imprimir_tablero(PartidaKomikan *partida) {
    pthread_mutex_lock(&partida->mutex_tablero); // <-- CORREGIDO: Usar el mutex correcto
    printf("\n ╰┈➤ESTADO DEL TABLERO KOMIKAN ᶻ 𝗓 𐰁\n");
    for (int i = 0; i < 6; i++) { // Iteramos sobre las filas del tablero
        for (int j = 0; j < 5; j++) { // Iteramos sobre las columnas del tablero
            if (i == 0 && j != 2) { printf("          "); continue; }
            if (partida->tablero[i][j] == VACIO) printf(". ⋆. 𐙚 ˚"); // Representamos las casillas vacías con un punto
            else if (partida->tablero[i][j] == PUMA) printf("P .⋆❀°"); // Representamos al PUMA con una "P"
            else if (partida->tablero[i][j] == PERRO) printf("D ⋆✴︎˚｡⋆"); // Representamos a los PERROS con una "D"
        }
        printf("\n"); // Nueva línea después de cada fila del tablero
    }
    // Imprimimos un mensaje final para indicar el final del tablero
    printf("🃜🃚🃖🃁🂭🂺\n");
    pthread_mutex_unlock(&partida->mutex_tablero); // <-- CORREGIDO: Desbloquear el mutex correcto
    // Unlock, shugo chara reference xd
}

// Función para validar un movimiento en el juego
int validar_movimiento(PartidaKomikan *partida, int tipo_ficha, Posicion origen, Posicion destino) { 
    // 1. Verificar que las posiciones de origen y destino estén dentro de los límites del tablero
    if (origen.x < 0 || origen.x >= 6 || origen.y < 0 || origen.y >= 5) return 0;
    if (origen.x == 0 && origen.y != 2) return 0;
    if (partida->tablero[origen.x][origen.y] != tipo_ficha) return 0;
    if (destino.x < 0 || destino.x >= 6 || destino.y < 0 || destino.y >= 5) return 0;
    if (destino.x == 0 && destino.y != 2) return 0;

    // 2. Verificar que la casilla de destino esté vacía
    if (partida->tablero[destino.x][destino.y] != VACIO) return 0;

    // 3. Verificar que el movimiento sea válido según las reglas del juego
    int diff_x = abs(destino.x - origen.x);
    int diff_y = abs(destino.y - origen.y);

    // Movimiento normal (una casilla en cualquier dirección)
    if (diff_x <= 1 && diff_y <= 1 && (diff_x != 0 || diff_y != 0)) {
        return 1;
    }

    // Movimiento con captura (el PUMA puede saltar sobre un PERRO)
    if (tipo_ficha == PUMA) {
        if ((diff_x == 2 && diff_y == 0) || (diff_x == 0 && diff_y == 2) || (diff_x == 2 && diff_y == 2)) { // Movimiento diagonal o recto de dos casillas
            // Calcular la posición intermedia entre origen y destino
            int medio_x = (origen.x + destino.x) / 2; 
            int medio_y = (origen.y + destino.y) / 2; 

            // Verificar si la casilla intermedia contiene un PERRO
            if (partida->tablero[medio_x][medio_y] == PERRO) {
                return 2; // Movimiento válido con captura
            }
        }
    }

    return 0; // Movimiento inválido
}

// Función para ejecutar el movimiento en el tablero de forma segura
/* Se invoca con el identificador de hilo para impedir que una ficha
   capturada vuelva a moverse como si fuese otro perro. */
int realizar_movimiento(PartidaKomikan *partida, int tipo_ficha, int id_ficha,
                       Posicion origen, Posicion destino) {
    pthread_mutex_lock(&partida->mutex_tablero); // Bloqueamos el mutex del tablero para proteger el acceso concurrente

    if (partida->juego_terminado || !partida->vivo[id_ficha] ||
        origen.x < 0 || origen.x >= 6 || origen.y < 0 || origen.y >= 5 ||
        partida->identidad[origen.x][origen.y] != id_ficha) {
        pthread_mutex_unlock(&partida->mutex_tablero);
        return 0;
    }

    int resultado_validacion = validar_movimiento(partida, tipo_ficha, origen, destino); // Validamos el movimiento antes de ejecutarlo

    if (resultado_validacion == 0) { // Si el movimiento es inválido, desbloqueamos el mutex y retornamos 0
        pthread_mutex_unlock(&partida->mutex_tablero);
        return 0; 
    }
    // Ejecutamos el movimiento actualizando el tablero
    partida->tablero[destino.x][destino.y] = tipo_ficha;
    partida->identidad[destino.x][destino.y] = id_ficha;
    partida->identidad[origen.x][origen.y] = -1;
    partida->tablero[origen.x][origen.y] = VACIO;
    // Si el movimiento fue una captura, eliminamos al PERRO capturado y actualizamos el contador de perros capturados
    if (resultado_validacion == 2) {
        int medio_x = (origen.x + destino.x) / 2;
        int medio_y = (origen.y + destino.y) / 2;
        int id_capturado = partida->identidad[medio_x][medio_y];
        if (id_capturado >= 1 && id_capturado <= 12)
            partida->vivo[id_capturado] = 0;
        partida->identidad[medio_x][medio_y] = -1;
        partida->tablero[medio_x][medio_y] = VACIO; // Eliminamos al PERRO capturado del tablero
        partida->perros_capturados++; // Incrementamos el contador de perros capturados por el PUMA
    }

    if (partida->perros_capturados >= 6) { // Si el PUMA ha capturado 6 PERROS, marcamos el juego como terminado
        partida->juego_terminado = 1;
        partida->ganador = PUMA;
    }

    /* Perros ganan si el Puma no tiene ni desplazamientos ni capturas.
       Se evalua dentro del mismo mutex para ver un tablero consistente. */
    if (!partida->juego_terminado) {
        Posicion puma = {-1, -1};
        for (int i = 0; i < 6; i++)
            for (int j = 0; j < 5; j++)
                if (partida->identidad[i][j] == 0)
                    puma = (Posicion){i, j};
        int puede_mover = 0;
        if (puma.x != -1) {
            for (int dx = -2; dx <= 2 && !puede_mover; dx++) {
                for (int dy = -2; dy <= 2; dy++) {
                    Posicion d = {puma.x + dx, puma.y + dy};
                    if (validar_movimiento(partida, PUMA, puma, d)) {
                        puede_mover = 1;
                        break;
                    }
                }
            }
        }
        if (!puede_mover) {
            partida->juego_terminado = 1;
            partida->ganador = PERRO;
        }
    }

    partida->movimientos++;
    pthread_mutex_unlock(&partida->mutex_tablero); // Desbloqueamos el mutex del tablero después de realizar el movimiento
    return resultado_validacion; // Retornamos el resultado de la validación (1 para movimiento normal, 2 para captura)
}

void* rutina_ficha(void *arg) { // Rutina para los hilos de las fichas (PUMA y PERROS)
    ArgHiloFicha *datos = (ArgHiloFicha *)arg; // Convertimos el puntero genérico a un puntero de tipo ArgHiloFicha
    PartidaKomikan *partida = datos->partida; // Obtenemos el puntero a la partida de Komikan
    
    // Inicializamos la semilla para la generación de números aleatorios usando el tiempo actual y el ID del hilo
    unsigned int seed = time(NULL) ^ (unsigned long)pthread_self();

    while (1) { // Bucle infinito para que la ficha siga intentando moverse hasta que el juego termine (como detesto los while 1)
        // Verificamos si el juego ha terminado antes de intentar movernos
        pthread_mutex_lock(&partida->mutex_tablero); // Bloqueamos el mutex del tablero para acceder a la bandera de juego terminado
        if (partida->juego_terminado || !partida->vivo[datos->id_ficha] ||
            (partida->cancelar && partida->cancelar())) { // Si el juego ha terminado, salimos del bucle
            pthread_mutex_unlock(&partida->mutex_tablero); // Desbloqueamos el mutex antes de salir, sino pos obvio marcará erroreishon lol
            break; // Salimos del bucle si el juego ha terminado
        }
        pthread_mutex_unlock(&partida->mutex_tablero); // Desbloqueamos el mutex del tablero después de verificar la bandera de juego terminado

        // Esperamos un tiempo aleatorio entre 50 y 300 milisegundos antes de intentar movernos (para simular el tiempo de reacción de la ficha)
        int milisegundos = 50 + (rand_r(&seed) % 251);
        struct timespec tiempo;

        tiempo.tv_sec = milisegundos / 1000;
        tiempo.tv_nsec = (milisegundos % 1000) * 1000000L;

        nanosleep(&tiempo, NULL); // Convertimos milisegundos a microsegundos para usleep

        // Generamos un movimiento aleatorio para la ficha (PUMA o PERRO)
        /* El Puma tambien debe intentar saltos de captura (distancia 2). */
        int salto = (datos->tipo_ficha == PUMA && rand_r(&seed) % 3 == 0) ? 2 : 1;
        int dir_x = ((int)(rand_r(&seed) % 3) - 1) * salto;
        int dir_y = ((int)(rand_r(&seed) % 3) - 1) * salto;

        if (dir_x == 0 && dir_y == 0) continue; // Si no hay movimiento, continuamos al siguiente ciclo del bucle

        Posicion destino = { // Calculamos la posición de destino sumando la dirección al origen
            .x = datos->pos_actual.x + dir_x,
            .y = datos->pos_actual.y + dir_y
        };

        // Intentamos realizar el movimiento de la ficha en la partida de Komikan
        int resultado = realizar_movimiento(partida, datos->tipo_ficha, datos->id_ficha, datos->pos_actual, destino);
        
        if (resultado > 0) { // Si el movimiento fue exitoso (1 para movimiento normal, 2 para captura)
            // Actualizamos la posición actual de la ficha en los datos del hilo
            datos->pos_actual = destino;
            
            // Log del movimiento; tablero completo cada 20 movimientos.
            pthread_mutex_lock(&partida->mutex_tablero);
            int numero = partida->movimientos;
            pthread_mutex_unlock(&partida->mutex_tablero);
            if (resultado == 2)
                printf("Puma capturo un perro.\n");
            if (numero % 20 == 0)
                imprimir_tablero(partida);
        }
    }

    return NULL;
}

// Función para destruir el juego y liberar recursos
void destruir_juego(PartidaKomikan *partida) {
    pthread_mutex_destroy(&partida->mutex_tablero);
}
/* Punto de entrada que llama el juez del servidor.
   Devuelve PUMA, PERRO, 0=empate por limite, -1=cancelacion/error. */
int ejecutar_partida_komikan(int (*cancelar)(void)) {
    PartidaKomikan partida;
    inicializar_juego(&partida);
    partida.cancelar = cancelar;

    pthread_t hilos[13];
    ArgHiloFicha args[13];
    int creados = 0;

    /* Asociar cada identidad con la ubicacion inicial de su ficha. */
    for (int id = 0; id < 13; id++) {
        args[id].partida = &partida;
        args[id].tipo_ficha = id == 0 ? PUMA : PERRO;
        args[id].id_ficha = id;
        args[id].pos_actual = (Posicion){-1, -1};
        for (int i = 0; i < 6; i++)
            for (int j = 0; j < 5; j++)
                if (partida.identidad[i][j] == id)
                    args[id].pos_actual = (Posicion){i, j};
    }

    imprimir_tablero(&partida);
    for (int id = 0; id < 13; id++) {
        int err = pthread_create(&hilos[id], NULL, rutina_ficha, &args[id]);
        if (err != 0) {
            fprintf(stderr, "No se pudo crear hilo de ficha %d: %d\n", id, err);
            pthread_mutex_lock(&partida.mutex_tablero);
            partida.juego_terminado = 1;
            pthread_mutex_unlock(&partida.mutex_tablero);
            break;
        }
        creados++;
    }

    /* Evita que partidas sin vencedor bloqueen mesas indefinidamente.
       El tiempo maximo es una politica de simulacion, no regla tradicional. */
    int transcurrido = 0;
    while (creados == 13) {
        pthread_mutex_lock(&partida.mutex_tablero);
        int terminado = partida.juego_terminado;
        pthread_mutex_unlock(&partida.mutex_tablero);
        if (terminado || (cancelar && cancelar()) || transcurrido >= 15000)
            break;
        struct timespec pausa = {0, 100000000L};
        nanosleep(&pausa, NULL);
        transcurrido += 100;
    }
    pthread_mutex_lock(&partida.mutex_tablero);
    partida.juego_terminado = 1;
    int ganador = partida.ganador;
    pthread_mutex_unlock(&partida.mutex_tablero);

    for (int i = 0; i < creados; i++)
        pthread_join(hilos[i], NULL);
    destruir_juego(&partida);

    if (creados != 13 || (cancelar && cancelar())) return -1;
    return ganador;
}
