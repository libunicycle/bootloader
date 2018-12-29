#pragma once

#include <efi/types.h>

// Loads ELF segments into memory and returns pointer to entry point
void *elf_load(void *data, size_t size);