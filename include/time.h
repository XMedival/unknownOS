#pragma once

#include <types.h>

typedef long time_t;
typedef long clock_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

#define CLOCKS_PER_SEC 1000000

time_t time(time_t *tloc);

/* Stubs - not implemented */
static inline clock_t clock(void) { return -1; }
static inline double difftime(time_t t1, time_t t0) { return (double)(t1 - t0); }
static inline struct tm *gmtime(const time_t *t) { (void)t; return 0; }
static inline struct tm *localtime(const time_t *t) { (void)t; return 0; }
static inline time_t mktime(struct tm *tm) { (void)tm; return -1; }
static inline size_t strftime(char *s, size_t max, const char *fmt, const struct tm *tm) {
    (void)s; (void)max; (void)fmt; (void)tm;
    return 0;
}
