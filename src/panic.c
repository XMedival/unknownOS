#include <panic.h>
#include <EGA.h>
#include <serial.h>
#include <x86.h>

__attribute__((noreturn))
void panic(const char* file, int line, const char* func, const char* expr) {
	printf("=== KERNEL PANIC ===\n");
	printf("FILE: %s\n", file);
	printf("LINE: %d\n", line);
	printf("FUNC: %s\n", func);
	printf("EXPR: %s\n", expr);
	asm volatile("xchg %bx,%bx");
	for (;;) asm volatile("cli\n""hlt\n");
}
