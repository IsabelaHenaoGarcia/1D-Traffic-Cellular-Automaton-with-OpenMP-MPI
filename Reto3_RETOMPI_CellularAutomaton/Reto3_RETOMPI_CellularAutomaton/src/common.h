#ifndef TRAFFIC_COMMON_H
#define TRAFFIC_COMMON_H

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef int cell_t;

typedef struct {
    long n;
    long steps;
    double density;
    unsigned long seed;
    int threads;
    int schedule;
    long chunk;
    int verbose;
    const char *csv;
    const char *tag;
} traffic_opts_t;

#define SCHED_STATIC  0
#define SCHED_DYNAMIC 1
#define SCHED_GUIDED  2
#define SCHED_AUTO    3

static inline double wall_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static inline uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static inline double rng01(uint32_t *state) {
    return (double)xorshift32(state) / (double)UINT32_MAX;
}

static long initialize_road(cell_t *road, long n, double density,
                            unsigned long seed) {
    uint32_t state = (uint32_t)(seed ? seed : 0xC0FFEEu);
    long ncars = 0;
    for (long i = 1; i <= n; ++i) {
        if (rng01(&state) < density) {
            road[i] = 1;
            ++ncars;
        } else {
            road[i] = 0;
        }
    }
    road[0] = road[n];
    road[n + 1] = road[1];
    return ncars;
}

static void parse_args(int argc, char **argv, traffic_opts_t *o) {
    o->n = 1000000;
    o->steps = 200;
    o->density = 0.5;
    o->seed = 42;
    o->threads = 0;
    o->schedule = SCHED_STATIC;
    o->chunk = 0;
    o->verbose = 0;
    o->csv = NULL;
    o->tag = "run";

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!strcmp(a, "-n") && i + 1 < argc) {
            o->n = atol(argv[++i]);
        } else if (!strcmp(a, "-s") && i + 1 < argc) {
            o->steps = atol(argv[++i]);
        } else if (!strcmp(a, "-d") && i + 1 < argc) {
            o->density = atof(argv[++i]);
        } else if (!strcmp(a, "-r") && i + 1 < argc) {
            o->seed = strtoul(argv[++i], NULL, 10);
        } else if (!strcmp(a, "-t") && i + 1 < argc) {
            o->threads = atoi(argv[++i]);
        } else if (!strcmp(a, "-S") && i + 1 < argc) {
            const char *s = argv[++i];
            if (!strcmp(s, "static"))       o->schedule = SCHED_STATIC;
            else if (!strcmp(s, "dynamic")) o->schedule = SCHED_DYNAMIC;
            else if (!strcmp(s, "guided"))  o->schedule = SCHED_GUIDED;
            else if (!strcmp(s, "auto"))    o->schedule = SCHED_AUTO;
        } else if (!strcmp(a, "-c") && i + 1 < argc) {
            o->chunk = atol(argv[++i]);
        } else if (!strcmp(a, "-v")) {
            o->verbose = 1;
        } else if (!strcmp(a, "-o") && i + 1 < argc) {
            o->csv = argv[++i];
        } else if (!strcmp(a, "--tag") && i + 1 < argc) {
            o->tag = argv[++i];
        } else if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
            fprintf(stderr,
                    "Usage: %s [-n N] [-s steps] [-d density] [-r seed] "
                    "[-t threads] [-S static|dynamic|guided|auto] [-c chunk] "
                    "[-o csv] [--tag name] [-v]\n", argv[0]);
            exit(0);
        }
    }
    if (o->density < 0.0) o->density = 0.0;
    if (o->density > 1.0) o->density = 1.0;
    if (o->n <= 0) o->n = 1;
    if (o->steps <= 0) o->steps = 1;
}

static void append_csv(const char *path, const char *header, const char *line) {
    FILE *fp = fopen(path, "a");
    if (!fp) return;
    long pos = ftell(fp);
    if (pos == 0 && header) fprintf(fp, "%s\n", header);
    fprintf(fp, "%s\n", line);
    fclose(fp);
}

#endif
