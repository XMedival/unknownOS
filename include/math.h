#pragma once

/*
** Math library using x87 FPU for unknownOS
*/

#define HUGE_VAL (__builtin_huge_val())
#define INFINITY (__builtin_inf())
#define NAN (__builtin_nan(""))

#define M_PI 3.14159265358979323846
#define M_E  2.71828182845904523536

/* Basic operations */
double fabs(double x);
double fmod(double x, double y);
double floor(double x);
double ceil(double x);
double trunc(double x);
double round(double x);

/* Exponential and logarithmic */
double sqrt(double x);
double pow(double base, double exp);
double exp(double x);
double log(double x);
double log10(double x);
double log2(double x);
double ldexp(double x, int exp);
double frexp(double x, int *exp);

/* Trigonometric */
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);

/* Hyperbolic (implemented in terms of exp/log) */
double sinh(double x);
double cosh(double x);
double tanh(double x);

/* Utility */
double modf(double x, double *iptr);
int isnan(double x);
int isinf(double x);
int isfinite(double x);
