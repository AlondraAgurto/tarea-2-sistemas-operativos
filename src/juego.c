#include <stdio.h>
#include <stdlib.h>
#include "juego.h"

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
    pthread_mutex_lock(&partida->tablero[0][0]); // Bloqueamos el acceso al tablero para evitar condiciones de carrera
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
    pthread_mutex_unlock(&partida->tablero[0][0]); // Desbloqueamos el acceso al tablero después de imprimirlo
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