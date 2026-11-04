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
    int identidad[6][5]; // ID individual: Puma 0, Perros 1..12, -1 vacio
    int vivo[13]; // Permite detener a un perro capturado
    int ganador; // PUMA, PERRO o 0
    int movimientos; // Para mostrar el tablero periodicamente
    int (*cancelar)(void); // Callback de cancelacion del servidor
    pthread_mutex_t mutex_tablero; // Mutex para proteger el acceso al tablero en un entorno multihilo (no pasarán buajajaja)
    int perros_capturados; // Contador de perros capturados por el puma
    int juego_terminado; // Bandera para indicar si el juego ha terminado
} PartidaKomikan;

// Funciones para inicializar el juego, validar movimientos y mostrar el tablero
void inicializar_juego(PartidaKomikan *partida); // Inicializa el estado de la partida y el tablero
int validar_movimiento(PartidaKomikan *partida, int tipo_ficha, Posicion origen, Posicion destino); // Valida si un movimiento es legal según las reglas del juego
void imprimir_tablero(PartidaKomikan *partida); // Imprime el estado actual del tablero en la consola


void destruir_juego(PartidaKomikan *partida);

/* Funcion publica invocada desde el hilo juez del servidor */
int ejecutar_partida_komikan(int (*cancelar)(void));

#endif // JUEGO_H
