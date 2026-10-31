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