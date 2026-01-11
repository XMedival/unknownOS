#pragma once
#include <types.h>

void kfree(char *v);
void freerange(void *start, void *end);
char *kalloc();
int freemem();
