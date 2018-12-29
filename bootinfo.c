#include "bootinfo.h"

#include "uniboot.h"

#ifndef UNIBOOT_NO_BUILTIN
#include <string.h>
#include "xefi.h"
#include <stdint.h>
#endif

void *buffer;
void *ptr;
size_t available;
size_t total_size;

#ifdef UNIBOOT_NO_BUILTIN
static void memset(uint8_t *p, uint8_t c, size_t n) {
    for (size_t i = 0; i < n; i++) {
        *p++ = c;
    }
}
#endif

void bootinfo_init(void *b, size_t size) {
    buffer = b;
    available = size;
    total_size = size;
    ptr = buffer;
    memset(ptr, 0, size);

    if (available < sizeof(struct uniboot_info)) {
        xefi_fatal("init_boot_info: provided buffer is too small for uniboot_info", EFI_BUFFER_TOO_SMALL);
    }

    struct uniboot_info *hdr = ptr;
    hdr->magic = UNIBOOT_MAGIC;
    hdr->version = 1;
    hdr->length = sizeof(struct uniboot_info);

    ptr += sizeof(struct uniboot_info);
}

void bootinfo_reinit(void *b, size_t size) {
    buffer = b;
    struct uniboot_info *hdr = buffer;
    total_size = size;
    ptr = buffer + hdr->length;
    available = size - hdr->length;
    memset(ptr, 0, size - hdr->length);
}

void *bootinfo_finalize(void) {
    return buffer;
}

void *bootinfo_alloc_size(size_t size) {
    if (size > available) {
        xefi_fatal("boot_info buffer is too small", EFI_BUFFER_TOO_SMALL);
    }

    void *p = ptr;
    ptr += size;
    available -= size;

    struct uniboot_info *hdr = buffer;
    hdr->length += size;

    return p;
}

size_t bootinfo_size_available(void) { return available; }

size_t bootinfo_size_consumed(void) { return total_size - available; }
