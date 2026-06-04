#include "common.h"
#include <omp.h>

#define BAD_CRITICAL    1
#define BAD_FALSESHARE  2
#define BAD_ATOMIC      3

static int bad_mode_from(const char *s) {
    if (!s) return BAD_CRITICAL;
    if (!strcmp(s, "critical"))   return BAD_CRITICAL;
    if (!strcmp(s, "falseshare")) return BAD_FALSESHARE;
    if (!strcmp(s, "atomic"))     return BAD_ATOMIC;
    return BAD_CRITICAL;
}

int main(int argc, char **argv) {
    traffic_opts_t opt;
    parse_args(argc, argv, &opt);

    const char *mode_str = "critical";
    for (int i = 1; i < argc - 1; ++i) {
        if (!strcmp(argv[i], "--bad")) {
            mode_str = argv[i + 1];
            break;
        }
    }
    const int bad = bad_mode_from(mode_str);

    if (opt.threads > 0) omp_set_num_threads(opt.threads);
    const int nthreads = omp_get_max_threads();

    const long n = opt.n;
    cell_t *old_road = (cell_t *)malloc((size_t)(n + 2) * sizeof(cell_t));
    cell_t *new_road = (cell_t *)malloc((size_t)(n + 2) * sizeof(cell_t));
    long *counters_bad = (long *)calloc((size_t)nthreads, sizeof(long));

    const long ncars = initialize_road(old_road, n, opt.density, opt.seed);
    const double actual_density = (double)ncars / (double)n;

    const double t0 = wall_seconds();

    double v_asymp_acc = 0.0;
    const long tail = opt.steps / 4 ? opt.steps / 4 : 1;

    for (long step = 0; step < opt.steps; ++step) {
        long nmove = 0;

        if (bad == BAD_CRITICAL) {
            #pragma omp parallel for schedule(static)
            for (long i = 1; i <= n; ++i) {
                if (old_road[i] == 0) {
                    new_road[i] = old_road[i - 1];
                } else {
                    new_road[i] = old_road[i + 1];
                    if (old_road[i + 1] == 0) {
                        #pragma omp critical
                        { nmove++; }
                    }
                }
            }
        } else if (bad == BAD_ATOMIC) {
            #pragma omp parallel for schedule(static)
            for (long i = 1; i <= n; ++i) {
                if (old_road[i] == 0) {
                    new_road[i] = old_road[i - 1];
                } else {
                    new_road[i] = old_road[i + 1];
                    if (old_road[i + 1] == 0) {
                        #pragma omp atomic
                        nmove++;
                    }
                }
            }
        } else {
            memset(counters_bad, 0, (size_t)nthreads * sizeof(long));
            #pragma omp parallel
            {
                const int tid = omp_get_thread_num();
                #pragma omp for schedule(static)
                for (long i = 1; i <= n; ++i) {
                    if (old_road[i] == 0) {
                        new_road[i] = old_road[i - 1];
                    } else {
                        new_road[i] = old_road[i + 1];
                        if (old_road[i + 1] == 0) counters_bad[tid]++;
                    }
                }
            }
            for (int t = 0; t < nthreads; ++t) nmove += counters_bad[t];
        }

        cell_t *tmp = old_road;
        old_road = new_road;
        new_road = tmp;
        old_road[0] = old_road[n];
        old_road[n + 1] = old_road[1];

        if (step >= opt.steps - tail) {
            v_asymp_acc += ncars > 0 ? (double)nmove / (double)ncars : 0.0;
        }
    }

    const double t1 = wall_seconds();
    const double elapsed = t1 - t0;
    const double v_asymp = v_asymp_acc / (double)tail;
    const double mcups = (double)n * (double)opt.steps / elapsed / 1e6;

    fprintf(stderr,
            "[omp_bad mode=%s T=%d] N=%ld steps=%ld density=%.3f "
            "v_asymp=%.5f time=%.4fs (%.2f MCUPS)\n",
            mode_str, nthreads, n, opt.steps, opt.density, v_asymp, elapsed,
            mcups);

    if (opt.csv) {
        char line[512];
        snprintf(line, sizeof(line),
                 "%s,omp_bad_%s,%ld,%ld,%.4f,%.5f,%.5f,%d,static,0,%.6f,%.2f",
                 opt.tag, mode_str, n, opt.steps, opt.density, actual_density,
                 v_asymp, nthreads, elapsed, mcups);
        append_csv(opt.csv,
                   "tag,version,N,steps,density,actual_density,v_asymp,"
                   "threads,schedule,chunk,time_s,mcups",
                   line);
    }

    free(counters_bad);
    free(old_road);
    free(new_road);
    return 0;
}
