#include "common.h"
#include <omp.h>

int main(int argc, char **argv) {
    traffic_opts_t opt;
    parse_args(argc, argv, &opt);

    if (opt.threads > 0) omp_set_num_threads(opt.threads);
    const int nthreads = omp_get_max_threads();

    const long n = opt.n;
    cell_t *old_road = (cell_t *)malloc((size_t)(n + 2) * sizeof(cell_t));
    cell_t *new_road = (cell_t *)malloc((size_t)(n + 2) * sizeof(cell_t));
    if (!old_road || !new_road) {
        fprintf(stderr, "alloc failed for N=%ld\n", n);
        return 1;
    }

    const long ncars = initialize_road(old_road, n, opt.density, opt.seed);
    const double actual_density = (double)ncars / (double)n;

    double *velocities = (double *)malloc((size_t)opt.steps * sizeof(double));
    const long chunk = opt.chunk > 0 ? opt.chunk : 0;
    const char *sched_name =
        opt.schedule == SCHED_DYNAMIC ? "dynamic" :
        opt.schedule == SCHED_GUIDED  ? "guided"  :
        opt.schedule == SCHED_AUTO    ? "auto"    : "static";

    const double t0 = wall_seconds();

    for (long step = 0; step < opt.steps; ++step) {
        long nmove = 0;

        switch (opt.schedule) {
        case SCHED_DYNAMIC:
            if (chunk > 0) {
                #pragma omp parallel for schedule(dynamic, 1) reduction(+:nmove)
                for (long i = 1; i <= n; ++i) {
                    if (old_road[i] == 0) {
                        new_road[i] = old_road[i - 1];
                    } else {
                        new_road[i] = old_road[i + 1];
                        if (old_road[i + 1] == 0) ++nmove;
                    }
                }
            } else {
                #pragma omp parallel for schedule(dynamic) reduction(+:nmove)
                for (long i = 1; i <= n; ++i) {
                    if (old_road[i] == 0) {
                        new_road[i] = old_road[i - 1];
                    } else {
                        new_road[i] = old_road[i + 1];
                        if (old_road[i + 1] == 0) ++nmove;
                    }
                }
            }
            break;
        case SCHED_GUIDED:
            #pragma omp parallel for schedule(guided) reduction(+:nmove)
            for (long i = 1; i <= n; ++i) {
                if (old_road[i] == 0) {
                    new_road[i] = old_road[i - 1];
                } else {
                    new_road[i] = old_road[i + 1];
                    if (old_road[i + 1] == 0) ++nmove;
                }
            }
            break;
        case SCHED_AUTO:
            #pragma omp parallel for schedule(auto) reduction(+:nmove)
            for (long i = 1; i <= n; ++i) {
                if (old_road[i] == 0) {
                    new_road[i] = old_road[i - 1];
                } else {
                    new_road[i] = old_road[i + 1];
                    if (old_road[i + 1] == 0) ++nmove;
                }
            }
            break;
        default:
            if (chunk > 0) {
                #pragma omp parallel for schedule(static, 1) reduction(+:nmove)
                for (long i = 1; i <= n; ++i) {
                    if (old_road[i] == 0) {
                        new_road[i] = old_road[i - 1];
                    } else {
                        new_road[i] = old_road[i + 1];
                        if (old_road[i + 1] == 0) ++nmove;
                    }
                }
            } else {
                #pragma omp parallel for schedule(static) reduction(+:nmove)
                for (long i = 1; i <= n; ++i) {
                    if (old_road[i] == 0) {
                        new_road[i] = old_road[i - 1];
                    } else {
                        new_road[i] = old_road[i + 1];
                        if (old_road[i + 1] == 0) ++nmove;
                    }
                }
            }
            break;
        }

        cell_t *tmp = old_road;
        old_road = new_road;
        new_road = tmp;
        old_road[0] = old_road[n];
        old_road[n + 1] = old_road[1];

        velocities[step] = ncars > 0 ? (double)nmove / (double)ncars : 0.0;
    }

    const double t1 = wall_seconds();
    const double elapsed = t1 - t0;

    double sum = 0.0;
    const long tail = opt.steps / 4 ? opt.steps / 4 : 1;
    for (long i = opt.steps - tail; i < opt.steps; ++i) sum += velocities[i];
    const double v_asymp = sum / (double)tail;

    long ncars_after = 0;
    #pragma omp parallel for reduction(+:ncars_after)
    for (long i = 1; i <= n; ++i) ncars_after += old_road[i];

    const double mcups = (double)n * (double)opt.steps / elapsed / 1e6;

    if (opt.verbose) {
        fprintf(stderr,
                "[omp T=%d sched=%s chunk=%ld] N=%ld steps=%ld density=%.3f "
                "v_asymp=%.5f time=%.4fs (%.2f MCUPS) cars=%ld->%ld\n",
                nthreads, sched_name, chunk, n, opt.steps, opt.density,
                v_asymp, elapsed, mcups, ncars, ncars_after);
    } else {
        printf("%d %ld %ld %.4f %.5f %.5f %.4f %.2f\n", nthreads, n, opt.steps,
               opt.density, actual_density, v_asymp, elapsed, mcups);
    }

    if (opt.csv) {
        char line[512];
        snprintf(line, sizeof(line),
                 "%s,omp,%ld,%ld,%.4f,%.5f,%.5f,%d,%s,%ld,%.6f,%.2f,%ld,%ld",
                 opt.tag, n, opt.steps, opt.density, actual_density, v_asymp,
                 nthreads, sched_name, chunk, elapsed, mcups, ncars,
                 ncars_after);
        append_csv(opt.csv,
                   "tag,version,N,steps,density,actual_density,v_asymp,"
                   "threads,schedule,chunk,time_s,mcups,ncars0,ncars1",
                   line);
    }

    free(velocities);
    free(old_road);
    free(new_road);
    return ncars != ncars_after;
}
