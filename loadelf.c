#include "loadelf.h"

#include "bootinfo.h"
#include "compiler.h"
#include "elf.h"
#include "string.h"
#include "uniboot.h"
#include "xefi.h"

static void elf_check_header(Elf64_Ehdr *hdr) {
    if (strncmp((char *)hdr->e_ident, ELFMAG, SELFMAG)) {
        xefi_fatal("app.elf: invalid ELF header magic value", EFI_LOAD_ERROR);
    }
    if (hdr->e_ident[EI_CLASS] != ELFCLASS64) {
        xefi_fatal("app.elf: only 64-bit ELF booting is supported", EFI_LOAD_ERROR);
    }
    if (hdr->e_type != ET_EXEC) {
        xefi_fatal("app.elf: only ELF executable file booting is supported", EFI_LOAD_ERROR);
    }
}

static void elf_load_segments(void *data) {
    Elf64_Ehdr *hdr = data;
    void *phdr_p = data + hdr->e_phoff;
    efi_physical_addr previous_end = 0; // we use it to track adjusted PT_LOAD segments that share the same page
    for (int i = 0; i < hdr->e_phnum; i++) {
        Elf64_Phdr *phdr = phdr_p;

        if (phdr->p_type != PT_LOAD)
            continue;

        efi_physical_addr addr = phdr->p_paddr;
        efi_physical_addr start = ROUND_DOWN(addr, PAGE_SIZE);
        efi_physical_addr end = ROUND_UP(addr + phdr->p_memsz, PAGE_SIZE);

        if (previous_end > start)
            start = previous_end;
        previous_end = end;

        size_t pages = (end - start) / PAGE_SIZE;
        if (pages) {
            efi_status r = gBS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &start);
            if (r) {
                xefi_fatal("load_elf: Cannot allocate buffer", r);
            }
        }

        // copy ELF segment data
        memcpy((void *)addr, data + phdr->p_offset, phdr->p_filesz);

        // the end of segment is zero-outed area (e.g. BSS)
        memset((void *)addr + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);

        phdr_p += hdr->e_phentsize;
    }
}

static void uniboot_populate_segments(void *data) {
    Elf64_Ehdr *hdr = data;

    size_t segment_list_size = sizeof(struct uniboot_segment_list) + hdr->e_phnum * sizeof(struct uniboot_segment);

    struct uniboot_entry *entry = bootinfo_alloc(struct uniboot_entry);
    entry->type = UNIBOOT_ENTRY_SEGMENT_LIST;
    entry->length = segment_list_size;

    struct uniboot_segment_list *segs = bootinfo_alloc_size(segment_list_size);
    segs->num = hdr->e_phnum;

    void *phdr_p = data + hdr->e_phoff;
    for (int i = 0; i < hdr->e_phnum; i++) {
        Elf64_Phdr *phdr = phdr_p;

        segs->segments[i].type = phdr->p_type;
        segs->segments[i].flags = phdr->p_flags;
        segs->segments[i].offset = phdr->p_offset;
        segs->segments[i].vaddr = phdr->p_vaddr;
        segs->segments[i].paddr = phdr->p_paddr;
        segs->segments[i].filesz = phdr->p_filesz;
        segs->segments[i].memsz = phdr->p_memsz;
        segs->segments[i].align = phdr->p_align;

        phdr_p += hdr->e_phentsize;
    }
}

static void uniboot_populate_sections(void *data) {
    Elf64_Ehdr *hdr = data;

    size_t section_list_size = sizeof(struct uniboot_section_list) + hdr->e_shnum * sizeof(struct uniboot_section);

    struct uniboot_entry *entry = bootinfo_alloc(struct uniboot_entry);
    entry->type = UNIBOOT_ENTRY_SECTION_LIST;
    entry->length = section_list_size;

    struct uniboot_section_list *list = bootinfo_alloc_size(section_list_size);
    list->num = hdr->e_shnum;
    struct uniboot_section *sec = list->sections;

    void *shdr_p = data + hdr->e_shoff;
    for (int i = 0; i < hdr->e_shnum; i++) {
        Elf64_Shdr *shdr = shdr_p;

        sec[i].name = shdr->sh_name;
        sec[i].type = shdr->sh_type;
        sec[i].flags = shdr->sh_flags;
        sec[i].addr = shdr->sh_addr;
        sec[i].size = shdr->sh_size;
        sec[i].addralign = shdr->sh_addralign;
        sec[i].entsize = shdr->sh_entsize;


        shdr_p += hdr->e_shentsize;
    }
}

void *elf_load(void *data, size_t size) {
    if (!data) {
        xefi_fatal("xefi_load_file", EFI_LOAD_ERROR);
    }
    if (size < sizeof(Elf64_Ehdr)) {
        xefi_fatal("app.elf size is too small", EFI_LOAD_ERROR);
    }

    Elf64_Ehdr *hdr = (Elf64_Ehdr *)data;
    elf_check_header(hdr);

    void *entry = (void *)hdr->e_entry;
    elf_load_segments(data);
    uniboot_populate_segments(data);
    uniboot_populate_sections(data);

    xefi_free(data, size);

    return entry;
}
