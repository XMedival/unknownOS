/*
** Math library using x87 FPU for unknownOS
** Uses inline assembly for FPU operations
*/

#include <math.h>

/* --- Basic operations --- */

double fabs(double x)
{
    double result;
    asm volatile("fabs" : "=t"(result) : "0"(x));
    return result;
}

double floor(double x)
{
    double result;
    unsigned short cw, cw_floor;

    /* Save control word and set rounding to floor (toward -inf) */
    asm volatile("fnstcw %0" : "=m"(cw));
    cw_floor = (cw & ~0x0C00) | 0x0400;  /* Round toward -infinity */
    asm volatile("fldcw %0" :: "m"(cw_floor));

    asm volatile("frndint" : "=t"(result) : "0"(x));

    /* Restore control word */
    asm volatile("fldcw %0" :: "m"(cw));
    return result;
}

double ceil(double x)
{
    double result;
    unsigned short cw, cw_ceil;

    asm volatile("fnstcw %0" : "=m"(cw));
    cw_ceil = (cw & ~0x0C00) | 0x0800;  /* Round toward +infinity */
    asm volatile("fldcw %0" :: "m"(cw_ceil));

    asm volatile("frndint" : "=t"(result) : "0"(x));

    asm volatile("fldcw %0" :: "m"(cw));
    return result;
}

double trunc(double x)
{
    double result;
    unsigned short cw, cw_trunc;

    asm volatile("fnstcw %0" : "=m"(cw));
    cw_trunc = (cw & ~0x0C00) | 0x0C00;  /* Round toward zero */
    asm volatile("fldcw %0" :: "m"(cw_trunc));

    asm volatile("frndint" : "=t"(result) : "0"(x));

    asm volatile("fldcw %0" :: "m"(cw));
    return result;
}

double round(double x)
{
    /* Round to nearest, ties to even (default FPU mode) */
    double result;
    unsigned short cw, cw_round;

    asm volatile("fnstcw %0" : "=m"(cw));
    cw_round = cw & ~0x0C00;  /* Round to nearest */
    asm volatile("fldcw %0" :: "m"(cw_round));

    asm volatile("frndint" : "=t"(result) : "0"(x));

    asm volatile("fldcw %0" :: "m"(cw));
    return result;
}

double fmod(double x, double y)
{
    double result;
    /* FPREM computes x - n*y where n is truncated quotient */
    asm volatile(
        "1: fprem\n"
        "   fnstsw %%ax\n"
        "   test $0x400, %%ax\n"
        "   jnz 1b"
        : "=t"(result)
        : "0"(x), "u"(y)
        : "ax"
    );
    return result;
}

/* --- Square root --- */

double sqrt(double x)
{
    double result;
    asm volatile("fsqrt" : "=t"(result) : "0"(x));
    return result;
}

/* --- Exponential and logarithmic --- */

double log2(double x)
{
    double result;
    /* FYL2X: result = 1.0 * log2(x) */
    asm volatile(
        "fld1\n"
        "fxch\n"
        "fyl2x"
        : "=t"(result)
        : "0"(x)
    );
    return result;
}

double log(double x)
{
    /* ln(x) = log2(x) / log2(e) = log2(x) * ln(2) */
    double result;
    asm volatile(
        "fldln2\n"       /* st(0) = ln(2) */
        "fxch\n"         /* st(0) = x, st(1) = ln(2) */
        "fyl2x"          /* st(0) = ln(2) * log2(x) = ln(x) */
        : "=t"(result)
        : "0"(x)
    );
    return result;
}

double log10(double x)
{
    /* log10(x) = log2(x) / log2(10) = log2(x) * log10(2) */
    double result;
    asm volatile(
        "fldlg2\n"       /* st(0) = log10(2) */
        "fxch\n"         /* st(0) = x, st(1) = log10(2) */
        "fyl2x"          /* st(0) = log10(2) * log2(x) = log10(x) */
        : "=t"(result)
        : "0"(x)
    );
    return result;
}

double exp(double x)
{
    /*
     * exp(x) = 2^(x * log2(e))
     * We use: 2^n = 2^int(n) * 2^frac(n)
     * And f2xm1 computes 2^x - 1 for |x| <= 1
     */
    double result;
    asm volatile(
        "fldl2e\n"           /* st(0) = log2(e) */
        "fmulp\n"            /* st(0) = x * log2(e) */
        "fld %%st(0)\n"      /* duplicate */
        "frndint\n"          /* st(0) = int part */
        "fxch\n"             /* st(0) = x*log2e, st(1) = int */
        "fsub %%st(1), %%st(0)\n"  /* st(0) = frac part */
        "f2xm1\n"            /* st(0) = 2^frac - 1 */
        "fld1\n"             /* st(0) = 1 */
        "faddp\n"            /* st(0) = 2^frac */
        "fscale\n"           /* st(0) = 2^frac * 2^int = 2^(x*log2e) */
        "fstp %%st(1)"       /* pop the int part */
        : "=t"(result)
        : "0"(x)
    );
    return result;
}

double pow(double base, double exponent)
{
    /* base^exp = 2^(exp * log2(base)) */
    if (base == 0.0) return 0.0;
    if (exponent == 0.0) return 1.0;

    double result;
    asm volatile(
        /* Calculate exp * log2(base) */
        "fxch\n"             /* st(0) = base, st(1) = exp */
        "fyl2x\n"            /* st(0) = exp * log2(base) */

        /* Now calculate 2^st(0) */
        "fld %%st(0)\n"      /* duplicate */
        "frndint\n"          /* st(0) = int part */
        "fxch\n"
        "fsub %%st(1), %%st(0)\n"  /* st(0) = frac part */
        "f2xm1\n"            /* st(0) = 2^frac - 1 */
        "fld1\n"
        "faddp\n"            /* st(0) = 2^frac */
        "fscale\n"           /* st(0) = result */
        "fstp %%st(1)"
        : "=t"(result)
        : "0"(exponent), "u"(base)
    );
    return result;
}

double ldexp(double x, int exp)
{
    double result;
    double e = (double)exp;
    asm volatile(
        "fscale\n"
        "fstp %%st(1)"
        : "=t"(result)
        : "0"(x), "u"(e)
    );
    return result;
}

double frexp(double x, int *exp)
{
    if (x == 0.0) {
        *exp = 0;
        return 0.0;
    }

    /* Extract exponent using fxtract */
    double mantissa, exponent;
    asm volatile(
        "fxtract"
        : "=t"(mantissa), "=u"(exponent)
        : "0"(x)
    );

    /* frexp returns mantissa in [0.5, 1.0), fxtract gives [1.0, 2.0) */
    *exp = (int)exponent + 1;
    return mantissa * 0.5;
}

/* --- Trigonometric --- */

double sin(double x)
{
    double result;
    asm volatile("fsin" : "=t"(result) : "0"(x));
    return result;
}

double cos(double x)
{
    double result;
    asm volatile("fcos" : "=t"(result) : "0"(x));
    return result;
}

double tan(double x)
{
    double result;
    asm volatile(
        "fptan\n"
        "fstp %%st(0)"  /* pop the 1.0 that fptan leaves */
        : "=t"(result)
        : "0"(x)
    );
    return result;
}

double atan(double x)
{
    double result;
    asm volatile(
        "fld1\n"
        "fpatan"
        : "=t"(result)
        : "0"(x)
    );
    return result;
}

double atan2(double y, double x)
{
    double result;
    asm volatile(
        "fpatan"
        : "=t"(result)
        : "0"(x), "u"(y)
    );
    return result;
}

double asin(double x)
{
    /* asin(x) = atan(x / sqrt(1 - x^2)) */
    double tmp = sqrt(1.0 - x * x);
    if (tmp == 0.0) return (x > 0) ? M_PI / 2 : -M_PI / 2;
    return atan(x / tmp);
}

double acos(double x)
{
    /* acos(x) = atan(sqrt(1 - x^2) / x) for x > 0 */
    /* acos(x) = pi + atan(sqrt(1 - x^2) / x) for x < 0 */
    /* acos(0) = pi/2 */
    if (x == 0.0) return M_PI / 2;
    double tmp = sqrt(1.0 - x * x);
    double a = atan(tmp / x);
    return (x < 0) ? M_PI + a : a;
}

/* --- Hyperbolic --- */

double sinh(double x)
{
    double e = exp(x);
    return (e - 1.0 / e) / 2.0;
}

double cosh(double x)
{
    double e = exp(x);
    return (e + 1.0 / e) / 2.0;
}

double tanh(double x)
{
    double e2 = exp(2.0 * x);
    return (e2 - 1.0) / (e2 + 1.0);
}

/* --- Utility --- */

double modf(double x, double *iptr)
{
    double i = trunc(x);
    *iptr = i;
    return x - i;
}

int isnan(double x)
{
    /* NaN is the only value not equal to itself */
    return x != x;
}

int isinf(double x)
{
    return (x == INFINITY) || (x == -INFINITY);
}

int isfinite(double x)
{
    return !isnan(x) && !isinf(x);
}
