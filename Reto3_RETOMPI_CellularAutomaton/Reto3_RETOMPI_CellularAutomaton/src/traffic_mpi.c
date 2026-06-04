#include "common.h"
#include <mpi.h>

#define ASYNC_MODE 1

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    traffic_opts_t opt;
    // parse_args from common.h will be handled collectively, 
    // but rank 0 can do it or all can do it. All processes have argc/argv.
    parse_args(argc, argv, &opt);

    // Sync mode override check via environment or define
    int use_async = 1;
    if (getenv("SYNC_MODE")) {
        use_async = 0;
    }

    const long global_n = opt.n;
    if (global_n < size) {
        if (rank == 0) fprintf(stderr, "Error: N < P no soportado para partición simple.\n");
        MPI_Finalize();
        return 1;
    }

    // Calcular el bloque local para cada proceso
    long local_n = global_n / size;
    long remainder = global_n % size;
    // Distribuir el resto en el último proceso (rank == size - 1)
    if (rank == size - 1) {
        local_n += remainder;
    }

    // local_old y local_new con 2 ghost cells (0 izq, local_n+1 der)
    cell_t *local_old = (cell_t *)malloc((size_t)(local_n + 2) * sizeof(cell_t));
    cell_t *local_new = (cell_t *)malloc((size_t)(local_n + 2) * sizeof(cell_t));
    if (!local_old || !local_new) {
        fprintf(stderr, "alloc failed on rank %d\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Inicializar celdas propias (índices 1..local_n)
    // Para asegurar la misma distribución que el serial, cada rank 
    // usa un RNG independiente pero esto cambia la semántica total 
    // si no inicializamos globalmente. Para ser simples, cada rank 
    // usa la semilla + rank para variar.
    // Aunque, lo ideal para reproducibilidad estricta es generar global y hacer scatter.
    // La actividad pide: inicializar array aleatorio con rho, usar srand...
    
    // Inicialización distribuida local:
    uint32_t state = (uint32_t)(opt.seed ? opt.seed : 0xC0FFEEu) + rank;
    long local_cars = 0;
    for (long i = 1; i <= local_n; ++i) {
        if (rng01(&state) < opt.density) {
            local_old[i] = 1;
            ++local_cars;
        } else {
            local_old[i] = 0;
        }
    }

    long global_cars = 0;
    MPI_Allreduce(&local_cars, &global_cars, 1, MPI_LONG, MPI_SUM, MPI_COMM_WORLD);
    
    const double actual_density = (double)global_cars / (double)global_n;
    if (rank == 0 && opt.verbose) {
        fprintf(stderr, "[mpi] N=%ld P=%d steps=%ld density=%.3f -> actual=%.4f async=%d\n", 
                global_n, size, opt.steps, opt.density, actual_density, use_async);
    }

    int left_rank = (rank - 1 + size) % size;
    int right_rank = (rank + 1) % size;

    const int TAG_RIGHT = 1;
    const int TAG_LEFT = 2;

    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();

    double v_asymp = 0.0;

    for (long step = 0; step < opt.steps; ++step) {
        // --- COMUNICACIÓN DE GHOST CELLS ---
        if (use_async) {
            MPI_Request reqs[4];
            // Recibir ghost izquierda (del left_rank enviado a su derecha) y derecha (del right_rank enviado a su izq)
            MPI_Irecv(&local_old[0], 1, MPI_INT, left_rank, TAG_RIGHT, MPI_COMM_WORLD, &reqs[0]);
            MPI_Irecv(&local_old[local_n + 1], 1, MPI_INT, right_rank, TAG_LEFT, MPI_COMM_WORLD, &reqs[1]);
            // Enviar borde izquierdo al ghost derecho del left_rank, y borde derecho al ghost izq del right_rank
            MPI_Isend(&local_old[1], 1, MPI_INT, left_rank, TAG_LEFT, MPI_COMM_WORLD, &reqs[2]);
            MPI_Isend(&local_old[local_n], 1, MPI_INT, right_rank, TAG_RIGHT, MPI_COMM_WORLD, &reqs[3]);
            MPI_Waitall(4, reqs, MPI_STATUSES_IGNORE);
        } else {
            // Síncrono Sendrecv
            // Paso 1: Enviar al vecino derecho, recibir del vecino izquierdo
            MPI_Sendrecv(&local_old[local_n], 1, MPI_INT, right_rank, TAG_RIGHT,
                         &local_old[0], 1, MPI_INT, left_rank, TAG_RIGHT,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            // Paso 2: Enviar al vecino izquierdo, recibir del vecino derecho
            MPI_Sendrecv(&local_old[1], 1, MPI_INT, left_rank, TAG_LEFT,
                         &local_old[local_n + 1], 1, MPI_INT, right_rank, TAG_LEFT,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        // --- ACTUALIZACIÓN LOCAL ---
        long local_movers = 0;
        for (long i = 1; i <= local_n; ++i) {
            if (local_old[i] == 0) {
                local_new[i] = local_old[i - 1];
            } else {
                local_new[i] = local_old[i + 1];
                if (local_old[i + 1] == 0) ++local_movers;
            }
        }

        cell_t *tmp = local_old;
        local_old = local_new;
        local_new = tmp;

        // --- REDUCCIÓN GLOBAL (frecuencia reducida por eficiencia o en cada paso si se necesita exacto) ---
        // La actividad pide imprimir de vez en cuando, pero calculamos velocidad media final.
        // Haremos la reducción al final o cuando toque imprimir.
        if (step % 100 == 0 || step == opt.steps - 1) {
            long global_movers = 0;
            MPI_Reduce(&local_movers, &global_movers, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
            
            if (rank == 0) {
                double avg_velocity = global_cars > 0 ? (double)global_movers / (double)global_cars : 0.0;
                if (step % 100 == 0 && opt.verbose) {
                    fprintf(stderr, "Step %ld: avg_velocity = %.5f\n", step, avg_velocity);
                }
                if (step == opt.steps - 1) {
                    v_asymp = avg_velocity;
                }
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double t1 = MPI_Wtime();
    double elapsed = t1 - t0;
    
    // Validación: que el número total de autos se mantenga
    long local_cars_after = 0;
    for (long i = 1; i <= local_n; ++i) local_cars_after += local_old[i];
    long global_cars_after = 0;
    MPI_Reduce(&local_cars_after, &global_cars_after, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        const double mcups = (double)global_n * (double)opt.steps / elapsed / 1e6;
        if (opt.verbose) {
            fprintf(stderr,
                    "[mpi] N=%ld steps=%ld density=%.3f -> actual=%.4f "
                    "ncars0=%ld ncars1=%ld v_asymp=%.5f time=%.4fs (%.2f MCUPS)\n",
                    global_n, opt.steps, opt.density, actual_density, global_cars, global_cars_after,
                    v_asymp, elapsed, mcups);
            fprintf(stderr, "Total time: %.4f s\n", elapsed);
        } else {
            printf("%ld %ld %.4f %.5f %.5f %.4f %.2f\n", global_n, opt.steps,
                   opt.density, actual_density, v_asymp, elapsed, mcups);
        }

        if (opt.csv) {
            char line[512];
            snprintf(line, sizeof(line),
                     "%s,mpi_%s,%ld,%ld,%.4f,%.5f,%.5f,%d,static,0,%.6f,%.2f,%ld,%ld",
                     opt.tag, use_async ? "async" : "sync", global_n, opt.steps, opt.density, actual_density, v_asymp,
                     size, elapsed, mcups, global_cars, global_cars_after);
            append_csv(opt.csv,
                       "tag,version,N,steps,density,actual_density,v_asymp,"
                       "threads,schedule,chunk,time_s,mcups,ncars0,ncars1",
                       line);
        }
    }

    free(local_old);
    free(local_new);
    MPI_Finalize();
    return (rank == 0 && global_cars != global_cars_after);
}
