#pragma once
#include <types.h>

void* memset(void *dst, int c, size_t n);
int memcmp(const void *v1, const void *v2, size_t n);
void* memmove(void *dst, const void *src, size_t n);
void* memcpy(void *dst, const void *src, size_t n);
void* memchr(const void *s, int c, size_t n);
char* strncpy(char *s, const char *t, size_t n);
char* safestrcpy(char *s, const char *t, int n);
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char* strchr(const char *s, int c);
char* strrchr(const char *s, int c);
char* strstr(const char *haystack, const char *needle);
char* strcpy(char *dst, const char *src);
char* strpbrk(const char *s, const char *accept);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
int strcoll(const char *s1, const char *s2);
