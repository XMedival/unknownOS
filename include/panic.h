#pragma once

__attribute__((noreturn))
void panic(const char* file, int line, const char* func, const char* expr);
