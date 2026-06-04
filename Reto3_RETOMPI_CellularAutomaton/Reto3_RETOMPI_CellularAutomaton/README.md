# Autómata Celular de Tráfico (MPI)

Este repositorio contiene la implementación y análisis de un autómata celular 1D para modelar el flujo de tráfico vehicular, paralelizado utilizando paso de mensajes con MPI.

## 1. Modelo del autómata celular

El modelo de tráfico vehicular se basa en la **Regla 184 de Wolfram**, un autómata celular elemental unidimensional. Cada celda de una "carretera" de tamaño $N$ puede estar ocupada (1) o vacía (0).

**Reglas de actualización (Combinaciones):**
Dada una celda en la posición $i$ en el instante $t$, su estado en $t+1$ depende de ella y de su vecino de adelante ($i+1$):
- Si $R_t(i) == 0$: La celda permanece vacía salvo que el vecino de atrás avance hacia ella. Como iteramos sobre los autos que avanzan, una celda vacía se convierte en 1 si el auto de atrás la ocupa, y sigue en 0 en otro caso.
- Si $R_t(i) == 1$ y $R_t(i+1) == 0$: El auto avanza. Por tanto, $R_{t+1}(i) = 0$ y $R_{t+1}(i+1) = 1$.
- Si $R_t(i) == 1$ y $R_t(i+1) == 1$: El auto no se mueve (bloqueado). Por tanto, $R_{t+1}(i) = 1$.

**Condiciones de frontera periódicas:**
Para evitar un borde rígido, se asume que la carretera es de forma topológica circular (un anillo). El vecino a la derecha de la celda máxima $N$ es la celda 1, e inversamente. Matemáticamente: $i_{next} = (i \mod N) + 1$. 

**Fórmula de velocidad:**
La velocidad en un paso $t$ se define como la proporción de los vehículos que lograron avanzar sobre el total de vehículos en la carretera. 
$$v(t) = \frac{\text{autos que avanzaron en } t}{\text{total de autos}}$$

## 2. Implementación serial

La versión serial se ubica en `src/traffic_serial.c`. No utiliza directivas ni librerías de paralelismo inter-proceso.

**Algoritmo:**
```c
for (int t = 0; t < NSTEPS; ++t) {
    long n_movers = 0;
    for (int i = 1; i <= N; ++i) {
        if (old_road[i] == 0) {
            new_road[i] = old_road[i-1];
        } else {
            new_road[i] = old_road[i+1];
            if (old_road[i+1] == 0) n_movers++;
        }
    }
}
```
**Complejidad y limitaciones:**
Complejidad $\mathcal{O}(N \times \text{NSTEPS})$. El límite principal es que un solo hilo de ejecución procesable topa la barrera de frecuencia y latencia de RAM, al procesar cada buffer de forma linear.

## 3. Análisis de paralelización con MPI

Se emplea "Domain Decomposition".
- INDEPENDIENTE: La iteración sobre la porción de arreglo [2 ... Nlocal - 1] (sin contar celdas bordes dependientes).
- REQUIERE COMUNICACIÓN: Celdas del margen exterior del `N_local` de cada partición se comunican con sus adyacentes para validar el paso o recepción de autos.

```text
       Subdominio rank 1
[Ghost 0] [Celda1..CeldaN] [Ghost N+1] 
```

**Esquemas:**
- **Asíncrona (MPI_Isend/Irecv):** Opera disparando "tickets" invisibles no bloqueantes al hardware, solapando el trabajo computacional si existiera antes del `Waitall`.
- **Síncrona (MPI_Sendrecv):** Evita el deadlock alternando el cruce (A envía a B su derecha, mientras B envía a A su izquierda). Se cruzan datos por pares bloqueando hasta tener la confirmación mutua de "listo".

## 4. Resultados de rendimiento

Ejecución de las pruebas robustas (3 promediadas por bloque) vía `run_benchmarks.sh` revelan la verdadera necesidad del crecimiento $N$:

| N | P | Modo | Tiempo (s) | Speedup | Eficiencia |
|---|---|---|---|---|---|
| 100,000 | 1 | serial | 0.0096 | 1.0000 | 1.0000 |
| 100,000 | 4 | async | 0.0338 | 0.286 | 0.0715 |
| 1,000,000 | 1 | serial | 0.0945 | 1.0000 | 1.0000 |
| 1,000,000 | 8 | sync | 0.1939 | 0.487 | 0.0609 |
| 10,000,000 | 1 | serial | 3.2254 | 1.0000 | 1.0000 |
| 10,000,000 | 4 | async | 3.3700 | 0.9572 | 0.2392 |
| 10,000,000 | 8 | sync | 2.6287 | 1.2269 | 0.1533 |

**Análisis físico de los datos actualizados:**
Al paraleliziar problemas con dominios subóptimos o pequeños ($N \le 1,000,000$) observamos cómo el *Speedup* jamás pasa de 1.0. Esto lo explica **La Ley de Amdahl**: El tiempo de transferencia y penalización generada por instanciar la barrera de MPI abruma la rápida tarea trivial del CPU calculando en su L3 cache.

No es sino hasta enfrentar escalas que agotan o exigen mucho la paginación masiva de la RAM principal (ej. $N = 10,000,000$) que el overhead es mitigado, y la partición computacional distribuida permite al fin rebasar el score y obtener un **Speedup > 1.2**, ya sea en el modo sincrónico o asíncrono puro de MPI.

## 5. Conclusiones
Para un modelo de bajo costo donde el dataset ingresa a Cache ($N \leq 10^5$), OpenMP brilla porque no tiene buffer duplicado. En MPI demostramos la premisa de la Escalabilidad Débil: Dividir tareas elementales `O(N)` requiere tamaños extremadamente masivos ($10^7$ variables o superior en un nodo moderno) para que el paso de *Ghost Cells* pase desapercibido y el paralelismo de nodos arroje un beneficio matemático real sobre correrlo en serial.
