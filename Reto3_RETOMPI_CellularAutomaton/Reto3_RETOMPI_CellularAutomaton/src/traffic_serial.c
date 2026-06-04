#include "common.h"

int main(int argc, char **argv) {
    traffic_opts_t opt;
    parse_args(argc, argv, &opt);

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

    const double t0 = wall_seconds();

    for (long step = 0; step < opt.steps; ++step) {
        long nmove = 0;
        for (long i = 1; i <= n; ++i) {
            if (old_road[i] == 0) {
                new_road[i] = old_road[i - 1];
            } else {
                new_road[i] = old_road[i + 1];
                if (old_road[i + 1] == 0) ++nmove;
            }
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
    for (long i = 1; i <= n; ++i) ncars_after += old_road[i];

    const double mcups = (double)n * (double)opt.steps / elapsed / 1e6;

    if (opt.verbose) {
        fprintf(stderr,
                "[serial] N=%ld steps=%ld density=%.3f -> actual=%.4f "
                "ncars0=%ld ncars1=%ld v_asymp=%.5f time=%.4fs (%.2f MCUPS)\n",
                n, opt.steps, opt.density, actual_density, ncars, ncars_after,
                v_asymp, elapsed, mcups);
    } else {
        printf("%ld %ld %.4f %.5f %.5f %.4f %.2f\n", n, opt.steps,
               opt.density, actual_density, v_asymp, elapsed, mcups);
    }

    if (opt.csv) {
        char line[512];
        snprintf(line, sizeof(line),
                 "%s,serial,%ld,%ld,%.4f,%.5f,%.5f,1,static,0,%.6f,%.2f,%ld,%ld",
                 opt.tag, n, opt.steps, opt.density, actual_density, v_asymp,
                 elapsed, mcups, ncars, ncars_after);
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
