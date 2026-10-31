#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "juego.h"

typedef struct { // Estructura para pasar argumentos a los hilos de las fichas
    PartidaKomikan *partida; // Puntero a la partida de Komikan
    int tipo_ficha; // Tipo de ficha (PUMA o PERRO)
    Posicion pos_actual; // Posición actual de la ficha
} ArgHiloFicha;

void inicializar_juego(PartidaKomikan *partida) { // Inicializar el juego Komikan
    // Inicializar el mutex del tablero
    pthread_mutex_init(&partida->mutex_tablero, NULL);
    
    partida->perros_capturados = 0;
    partida->juego_terminado = 0;

    // Limpiar todo el tablero dejándolo en VACIO (0)
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 5; j++) {
            partida->tablero[i][j] = VACIO;
        }
    }

    // Colocamos al PUMA en una posición inicial (por ejemplo, en el centro del tablero)
    partida->tablero[2][2] = PUMA;

    // Colocamos a los PERROS en posiciones iniciales (por ejemplo, en las filas superiores)
    int perros_colocados = 0;
    for (int i = 0; i < 2; i++) { // Colocamos perros en las dos primeras filas
        for (int j = 0; j < 5; j++) { // Colocamos un perro en cada columna de las dos primeras filas
            if (perros_colocados < 12) {
                partida->tablero[i][j] = PERRO; // Colocamos un perro
                perros_colocados++;
            }
        }
    }
}
// Función para imprimir el estado actual del tablero
void imprimir_tablero(PartidaKomikan *partida) {
    pthread_mutex_lock(&partida->mutex_tablero); // <-- CORREGIDO: Usar el mutex correcto
    printf("\n ╰┈➤ESTADO DEL TABLERO KOMIKAN ᶻ 𝗓 𐰁\n");
    for (int i = 0; i < 6; i++) { // Iteramos sobre las filas del tablero
        for (int j = 0; j < 5; j++) { // Iteramos sobre las columnas del tablero
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
    if (destino.x < 0 || destino.x >= 6 || destino.y < 0 || destino.y >= 5) return 0;

    // 2. Verificar que la casilla de destino esté vacía
    if (partida->tablero[destino.x][destino.y] != VACIO) return 0;

    // 3. Verificar que el movimiento sea válido según las reglas del juego
    int diff_x = abs(destino.x - origen.x);
    int diff_y = abs(destino.y - origen.y);

    // Movimiento normal (una casilla en cualquier dirección)
    if (diff_x <= 1 && diff_y <= 1) {
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
int realizar_movimiento(PartidaKomikan *partida, int tipo_ficha, Posicion origen, Posicion destino) {
    pthread_mutex_lock(&partida->mutex_tablero); // Bloqueamos el mutex del tablero para proteger el acceso concurrente

    int resultado_validacion = validar_movimiento(partida, tipo_ficha, origen, destino); // Validamos el movimiento antes de ejecutarlo

    if (resultado_validacion == 0) { // Si el movimiento es inválido, desbloqueamos el mutex y retornamos 0
        pthread_mutex_unlock(&partida->mutex_tablero);
        return 0; 
    }
    // Ejecutamos el movimiento actualizando el tablero
    partida->tablero[destino.x][destino.y] = tipo_ficha;
    partida->tablero[origen.x][origen.y] = VACIO;
    // Si el movimiento fue una captura, eliminamos al PERRO capturado y actualizamos el contador de perros capturados
    if (resultado_validacion == 2) {
        int medio_x = (origen.x + destino.x) / 2;
        int medio_y = (origen.y + destino.y) / 2;
        partida->tablero[medio_x][medio_y] = VACIO; // Eliminamos al PERRO capturado del tablero
        partida->perros_capturados++; // Incrementamos el contador de perros capturados por el PUMA
    }

    if (partida->perros_capturados >= 6) { // Si el PUMA ha capturado 6 PERROS, marcamos el juego como terminado
        partida->juego_terminado = 1;
    }

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
        if (partida->juego_terminado) { // Si el juego ha terminado, salimos del bucle
            pthread_mutex_unlock(&partida->mutex_tablero); // Desbloqueamos el mutex antes de salir, sino pos obvio marcará erroreishon lol
            break; // Salimos del bucle si el juego ha terminado
        }
        pthread_mutex_unlock(&partida->mutex_tablero); // Desbloqueamos el mutex del tablero después de verificar la bandera de juego terminado

        // Esperamos un tiempo aleatorio entre 50 y 300 milisegundos antes de intentar movernos (para simular el tiempo de reacción de la ficha)
        int milisegundos = 50 + (rand_r(&seed) % 251);
        usleep(milisegundos * 1000); // Convertimos milisegundos a microsegundos para usleep

        // Generamos un movimiento aleatorio para la ficha (PUMA o PERRO)
        int dir_x = (rand_r(&seed) % 3) - 1; // -1, 0, o 1
        int dir_y = (rand_r(&seed) % 3) - 1; // -1, 0, o 1

        if (dir_x == 0 && dir_y == 0) continue; // Si no hay movimiento, continuamos al siguiente ciclo del bucle

        Posicion destino = { // Calculamos la posición de destino sumando la dirección al origen
            .x = datos->pos_actual.x + dir_x,
            .y = datos->pos_actual.y + dir_y
        };

        // Intentamos realizar el movimiento de la ficha en la partida de Komikan
        int resultado = realizar_movimiento(partida, datos->tipo_ficha, datos->pos_actual, destino);
        
        if (resultado > 0) { // Si el movimiento fue exitoso (1 para movimiento normal, 2 para captura)
            // Actualizamos la posición actual de la ficha en los datos del hilo
            datos->pos_actual = destino;
            
            // Imprimimos el estado actual del tablero después de cada movimiento exitoso
            imprimir_tablero(partida);
        }
    }

    free(datos);
    return NULL;
}