#pragma once

#include <stddef.h>

void bootinfo_init(void *buffer, size_t size);
void bootinfo_reinit(void *b, size_t size);

void *bootinfo_finalize(void);

void *bootinfo_alloc_size(size_t size);

#define bootinfo_alloc(type) ({ (type *)bootinfo_alloc_size(sizeof(type)); })

size_t bootinfo_size_available(void);

size_t bootinfo_size_consumed(void);