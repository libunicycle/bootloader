#pragma once

#define PAGE_SIZE 4096

#define ROUND_UP(n, d) ((n + (__typeof__(n))d - 1) & ~((__typeof__(n))d - 1))
#define ROUND_DOWN(n, d) ((n) & ~((__typeof__(n))d - 1))

#define BIT(x) ((uint64_t)1 << (x))