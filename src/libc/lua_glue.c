/*
** Lua integration glue for unknownOS
** Provides output functions and initialization helpers
*/

#include <types.h>
#include <EGA.h>
#include <lua_alloc.h>

/* Output functions required by luaconf.h */

void lua_writestring(const char *s, size_t l)
{
    while (l-- > 0) {
        putchar(*s++);
    }
}

void lua_writeline(void)
{
    putchar('\n');
}

void lua_writestringerror(const char *s, const char *p)
{
    /* Simple error output - just print the format string for now */
    /* A full implementation would handle %s substitution */
    printf("Lua error: ");
    while (*s) {
        if (*s == '%' && *(s+1) == 's' && p) {
            printf("%s", p);
            s += 2;
            p = NULL;  /* Only substitute once */
        } else {
            putchar(*s++);
        }
    }
    putchar('\n');
}
