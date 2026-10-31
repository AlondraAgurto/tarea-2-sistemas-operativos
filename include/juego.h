#ifndef JUEGO_H
#define JUEGO_H

#include <pthread.h>

// Definición de constantes para representar los tipos de fichas en el tablero
#define VACIO 0
#define PUMA 1
#define PERRO 2

// Estructura para representar una posición en el tablero
typedef struct {
    int x; // Coordenada x (fila)
    int y;  // Coordenada y (columna)
} Posicion;

// Estructura para representar el estado de la partida
typedef struct {
    int tablero[6][5]; // Matriz que representa el tablero de juego (6 filas x 5 columnas)
    pthread_mutex_t mutex_tablero; // Mutex para proteger el acceso al tablero en un entorno multihilo (no pasarán buajajaja)
    int perros_capturados; // Contador de perros capturados por el puma
    int juego_terminado; // Bandera para indicar si el juego ha terminado
} PartidaKomikan;

// Funciones para inicializar el juego, validar movimientos y mostrar el tablero
void inicializar_juego(PartidaKomikan *partida); // Inicializa el estado de la partida y el tablero
int validar_movimiento(PartidaKomikan *partida, int tipo_ficha, Posicion origen, Posicion destino); // Valida si un movimiento es legal según las reglas del juego
void imprimir_tablero(PartidaKomikan *partida); // Imprime el estado actual del tablero en la consola

#endif