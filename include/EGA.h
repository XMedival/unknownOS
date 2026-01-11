#pragma once
#include <types.h>

#define ATTRIBUTE 7

#define COLUMNS 80
#define LINES 24
#define FRAMEBUFFER_ADDR 0xB8000

void cls();
void itoa(char *buf, int base, int d);
void putchar(int c);
void printf(const char *format, ...);
