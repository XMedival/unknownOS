#pragma once

/*
** setjmp/longjmp for x86 freestanding
** jmp_buf layout: [ebx, esi, edi, ebp, esp, eip]
*/

typedef unsigned int jmp_buf[6];

/* Save current context, returns 0 on direct call, non-zero from longjmp */
int setjmp(jmp_buf env);

/* Restore context saved by setjmp, never returns */
void longjmp(jmp_buf env, int val) __attribute__((noreturn));
