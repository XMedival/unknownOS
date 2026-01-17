#pragma once
#include <EGA.h>

#define LOG_OK(msg, ...)   do { printf("[  OK  ] "); printf(msg, ##__VA_ARGS__); printf("\n"); } while(0)
#define LOG_INFO(msg, ...) do { printf("[ INFO ] "); printf(msg, ##__VA_ARGS__); printf("\n"); } while(0)
#define LOG_DEBG(msg, ...) do { printf("[ DEBG ] "); printf(msg, ##__VA_ARGS__); printf("\n"); } while(0)
#define LOG_WARN(msg, ...) do { printf("[ WARN ] "); printf(msg, ##__VA_ARGS__); printf("\n"); } while(0)
#define LOG_FAIL(msg, ...) do { printf("[ FAIL ] "); printf(msg, ##__VA_ARGS__); printf("\n"); } while(0)
