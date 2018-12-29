// SPDX-License-Identifier: MIT
// Some part of this code is based on Zircon bootloader

#include "bootinfo.h"
#include "compiler.h"
#include "config.h"
#include "framebuffer.h"
#include "loadelf.h"
#include "netboot.h"
#include "printf.h"
#include "string.h"
#include "uniboot.h"
#include <xefi.h>

static efi_guid AcpiTableGUID = ACPI_TABLE_GUID;
static efi_guid Acpi2TableGUID = ACPI_20_TABLE_GUID;
static uint8_t ACPI_RSD_PTR[8] = "RSD PTR ";

#define MSR_EXT_FEATURES 0xc0000080

// flags for MSR_EXT_FEATURES
#define MSR_EXT_FEATURES_LONG_MODE BIT(8) // Long mode (64 bits)
#define MSR_EXT_FEATURES_NO_EXECUTE BIT(11) // enables NXE paging bit

static uint64_t x86_rdmsr(uint32_t id) {
    uint32_t eax, edx;
    __asm__ volatile("rdmsr" : "=a"(eax), "=d"(edx) : "c"(id));

    uint64_t ret = edx;
    ret <<= 32;
    ret |= eax;

    return ret;
}

// write model specific register, set value into edx:eax
static void x86_wrmsr(uint32_t id, uint64_t val) {
    uint32_t eax = (uint32_t)val;
    uint32_t edx = (uint32_t)(val >> 32);
    __asm__ volatile("wrmsr" ::"c"(id), "a"(eax), "d"(edx));
}

static uint64_t find_acpi_root(efi_system_table *sys) {
    efi_configuration_table *cfgtab = sys->ConfigurationTable;

    for (size_t i = 0; i < sys->NumberOfTableEntries; i++) {
        if (xefi_cmp_guid(&cfgtab[i].VendorGuid, &AcpiTableGUID) && xefi_cmp_guid(&cfgtab[i].VendorGuid, &Acpi2TableGUID)) {
            // not an ACPI table
            continue;
        }
        if (memcmp(cfgtab[i].VendorTable, ACPI_RSD_PTR, 8)) {
            // not the Root Description Pointer
            continue;
        }
        return (uint64_t)cfgtab[i].VendorTable;
    }
    return 0;
}

static void read_acpi_root(efi_system_table *sys) {
    uint64_t acpi_root = find_acpi_root(sys);

    if (!acpi_root) {
        // Firmware does not provide ACPI root information
        return;
    }

    struct uniboot_entry *entry = bootinfo_alloc(struct uniboot_entry);
    entry->type = UNIBOOT_ENTRY_ACPI_INFO;
    entry->length = sizeof(struct uniboot_acpi_info);

    struct uniboot_acpi_info *acpi = bootinfo_alloc(struct uniboot_acpi_info);
    acpi->acpi_root = acpi_root;
}

// This functions exits BootServices and thus it should go last, right before jumping to OS
static void read_memory_map(void) {
    struct uniboot_entry *entry = bootinfo_alloc(struct uniboot_entry);
    entry->type = UNIBOOT_ENTRY_MEMORY_MAP;

    struct uniboot_memory_map *mmap = bootinfo_alloc(struct uniboot_memory_map);

    void *mmap_begin = mmap + 1;
    size_t mmap_size = bootinfo_size_available();
    size_t mmap_key = 0;
    size_t desc_size = 0;
    uint32_t desc_version = 0;
    efi_status r = gBS->GetMemoryMap(&mmap_size, (efi_memory_descriptor *)mmap_begin, &mmap_key, &desc_size, &desc_version);
    if (r) {
        xefi_fatal("GetMemoryMap", r);
    }
    if (desc_version != EFI_MEMORY_DESCRIPTOR_VERSION) {
        xefi_fatal("init_memory_map: only descriptor version 1 is supported for GetMemoryMap()", r);
    }

    size_t areas_num = mmap_size / desc_size;
    size_t areas_num_counter = 0;
    struct uniboot_memory_area *area = bootinfo_alloc_size(areas_num * sizeof(struct uniboot_memory_area));
    entry->length = sizeof(struct uniboot_memory_map) + areas_num * sizeof(struct uniboot_memory_area);

    for (void *ptr = mmap_begin; ptr < mmap_begin + mmap_size; ptr += desc_size) {
        efi_memory_descriptor *desc = (efi_memory_descriptor *)ptr;

        uint64_t type = 0;
        switch (desc->Type) {
        case EfiReservedMemoryType:
        case EfiRuntimeServicesCode:
        case EfiRuntimeServicesData:
        case EfiMemoryMappedIO:
        case EfiMemoryMappedIOPortSpace:
        case EfiPalCode:
            type = UNIBOOT_MEM_RESERVED;
            break;

        case EfiUnusableMemory:
            type = UNIBOOT_MEM_UNUSABLE;
            break;

        case EfiACPIReclaimMemory:
            type = UNIBOOT_MEM_ACPI;
            break;

        case EfiLoaderCode:
        case EfiLoaderData:
        case EfiBootServicesCode:
        case EfiBootServicesData:
        case EfiConventionalMemory:
            type = UNIBOOT_MEM_RAM;
            break;

        case EfiACPIMemoryNVS:
            type = UNIBOOT_MEM_NVS;
            break;

        default:
            printf("Invalid EFI memory descriptor type (0x%x)!\n", desc->Type);
            continue;
        }

        // check if memory areas are adjusted, merge it in this case
        if (areas_num_counter && (area - 1)->type == type && ((area - 1)->start + (area - 1)->length) == desc->PhysicalStart) {
            (area - 1)->length += desc->NumberOfPages * PAGE_SIZE;
        } else {
            area->type = type;
            area->start = desc->PhysicalStart;
            area->length = desc->NumberOfPages * PAGE_SIZE;
            area++;
            areas_num_counter++;
        }
    }
    mmap->num = areas_num_counter;

    r = gBS->ExitBootServices(gImg, mmap_key);
    if (r) {
        xefi_fatal("ExitBootServices", r);
    }
}

static __attribute__((noreturn)) void jump_to_app(uniboot_entry_point_t start_app, void *boot_info) {
#if __x86_64__
    __asm__ volatile("mov $0, %%rbp; "
                     "cli; " ::
                         : "rbp");
#else
#error "jump function is not defined"
#endif

    start_app(boot_info);

    __builtin_unreachable();
}

#define KBUFSIZE (32 * 1024 * 1024)
#define RBUFSIZE (512 * 1024 * 1024)

static nbfile nbapp;
nbfile *netboot_get_buffer(const char *name, size_t size) {
    if (!strcmp(name, NB_APP_FILENAME)) {
        return &nbapp;
    }
    return NULL;
}

void do_netboot() {
    efi_physical_addr mem = 0xFFFFFFFF;
    if (gBS->AllocatePages(AllocateMaxAddress, EfiLoaderData, KBUFSIZE / 4096, &mem)) {
        printf("Failed to allocate network io buffer\n");
        return;
    }
    nbapp.data = (void *)mem;
    nbapp.size = KBUFSIZE;

    printf("\nNetBoot Server Started...\n\n");
    efi_tpl prev_tpl = gBS->RaiseTPL(TPL_NOTIFY);
    while (true) {
        int n = netboot_poll();
        if (n < 1) {
            continue;
        }
        if (nbapp.offset < 4096) {
            // too small to be a kernel
            continue;
        }

        // make sure network traffic is not in flight, etc
        netboot_close();

        // Restore the TPL before booting the kernel, or failing to netboot
        gBS->RestoreTPL(prev_tpl);

        break;
    }
}

EFIAPI efi_status efi_main(efi_handle img, efi_system_table *sys) {
    xefi_init(img, sys);
    gConOut->ClearScreen(gConOut);
#ifdef OUTPUT_SERIAL
    xefi_init_serial();
#endif

    uint8_t bootinfo_buff[10240];
    bootinfo_init(bootinfo_buff, sizeof(bootinfo_buff));

    size_t cfg_size;
    char *cfg_file = xefi_load_file(L"bootloader.cfg", &cfg_size, 0);
    if (cfg_file) {
        config_init(cfg_file, cfg_size);
        xefi_free(cfg_file, cfg_size);
    }

    size_t elf_size = 0;
    void *elf_data = NULL;
    const char *boot = config_get("boot", "file");
    if (strcmp(boot, "network") == 0) {
        // See if there's a network interface
        const char *nodename = config_get("nodename", NULL);
        bool have_network = netboot_init(nodename) == 0;
        if (have_network) {
            printf("Nodename: %s\n", netboot_nodename());

            do_netboot();

            elf_data = nbapp.data;
            elf_size = nbapp.size;
        } else {
            printf("Network is not available, trying to load application from disk\n");
        }
    }
    if (!elf_data) {
        // local disk boot
        elf_data = xefi_load_file(L"app.elf", &elf_size, 0);
        if (!elf_data)
            xefi_fatal("Cannot load ap.elf", EFI_LOAD_ERROR);
    }
    uniboot_entry_point_t entry = elf_load(elf_data, elf_size);

    read_acpi_root(sys);
    read_framebuffer_info();
    read_memory_map();

    void *boot_info = bootinfo_finalize();

    uint64_t mode_msr = x86_rdmsr(MSR_EXT_FEATURES);
    mode_msr |= MSR_EXT_FEATURES_NO_EXECUTE;
    x86_wrmsr(MSR_EXT_FEATURES, mode_msr);


    jump_to_app(entry, boot_info);

    __builtin_unreachable();
}
