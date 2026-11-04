# Tarea 2: Sistemas Operativos — Torneo Komikan

Proyecto desarrollado en **C** bajo un entorno Unix, implementando el juego tradicional **Komikan** mediante programación concurrente, hilos POSIX (`pthread`) y exclusión mutua para la sincronización del tablero y el servidor de torneo.

## Integrantes
- Alondra Agurto Muñoz

- Joaquín Cortés


---

## Requisitos de Compilación
El proyecto cuenta con un `Makefile` configurado strictly bajo el estándar requerido:
- Compilador: `gcc`

- Opciones: `-Wall -Wextra -pthread -Iinclude`


## Instrucciones de Compilación y Ejecución

1. **Clonar el repositorio y entrar a la carpeta:**

   ```bash
   git clone git@github.com:AlondraAgurto/tarea-2-sistemas-operativos.git
   cd tarea-2-sistemas-operativos
   ```

2. **Compilar el programa:**

   ```bash
   make clean
   make
   ```

   Esto generará el ejecutable principal llamado `torneo`.

3. **Ejecutar el programa:**
   El programa recibe como parámetros obligatorios el número de jugadores (N, debe ser par) y el número máximo de mesas (M), además de un archivo de configuración opcional:

   ```bash
   ./torneo <N> <M> [config.txt]
   ```

   Ejemplo:

   ```bash
   ./torneo 4 2
   ```

---

## Decisiones de Diseño y Sincronización

### 1. Servidor y Gestión del Torneo (Joaquín Cortés)
- **Servidor Principal:** Recibe y procesa los $N$ jugadores participantes en el torneo.
- **Emparejamiento de Jugadores:** Se agrupan los participantes en parejas de dos en dos (asignando roles de Puma y Perros).
- **Control de Mesas Concurrentes:** Gestión del límite máximo de $M$ mesas simultáneas mediante espera pasiva con semáforos, bloqueando mesas adicionales hasta que una previa termine.
- **Administración de Jueces:** Creación y control de hilos para los jueces asignados a cada mesa de juego.
- **Manejo de Señales y Limpieza General:** Captura de la señal `Ctrl+C` (`SIGINT`) para ejecutar una salida segura, coordinando la liberación ordenada de mesas y recursos globales del servidor.

### 2. Motor del Juego y Concurrencia (Alondra Agurto Muñoz)
- **Tablero y Reglas Básicas:** Implementación del tablero del Komikan en una matriz compartida de 6x5 con posición inicial del Puma en el centro y 12 Perros en la zona superior.
- **Concurrencia e Hilos de Fichas (`pthreads`):** Cada ficha (Puma y Perros) funciona como un hilo autónomo con retardos aleatorios entre 50 y 300 ms (`usleep` y `rand_r`).
- **Protección del Tablero (Mutexes):** Control del acceso concurrente a la matriz compartida mediante `pthread_mutex_t` (`mutex_tablero`) para evitar condiciones de carrera durante los movimientos y capturas.
- **Validación y Capturas:** Lógica de movimiento en casillas adyacentes y regla de salto de captura del Puma.
- **Término y Limpieza de Partida:** Monitoreo del fin de partida (6 perros capturados) y liberación de recursos mediante `pthread_mutex_destroy` en `destruir_juego`.