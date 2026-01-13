#pragma once

/* Minimal locale.h stub for freestanding environment */

struct lconv {
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
};

static inline struct lconv *localeconv(void) {
    static struct lconv lc = { ".", "", "" };
    return &lc;
}

static inline char *setlocale(int category, const char *locale) {
    (void)category;
    (void)locale;
    return "C";
}

#define LC_ALL      0
#define LC_COLLATE  1
#define LC_CTYPE    2
#define LC_MONETARY 3
#define LC_NUMERIC  4
#define LC_TIME     5
