#include "framebuffer.h"

#include "bootinfo.h"
#include "printf.h"
#include "uniboot.h"
#include <efi/protocol/graphics-output.h>
#include <xefi.h>

static void get_bit_range(uint32_t mask, int *high, int *low) {
    *high = -1;
    *low = -1;
    int idx = 0;
    while (mask) {
        if (*low < 0 && (mask & 1))
            *low = idx;
        idx++;
        mask >>= 1;
    }
    *high = idx - 1;
}

static int fb_pixel_format_from_bitmask(efi_pixel_bitmask bitmask) {
    int r_hi = -1, r_lo = -1, g_hi = -1, g_lo = -1, b_hi = -1, b_lo = -1;

    get_bit_range(bitmask.RedMask, &r_hi, &r_lo);
    get_bit_range(bitmask.GreenMask, &g_hi, &g_lo);
    get_bit_range(bitmask.BlueMask, &b_hi, &b_lo);

    if (r_lo < 0 || g_lo < 0 || b_lo < 0) {
        goto unknown;
    }

    if ((r_hi == 23 && r_lo == 16) && (g_hi == 15 && g_lo == 8) && (b_hi == 7 && b_lo == 0)) {
        return UNIBOOT_PIXEL_FORMAT_RGB_x888;
    }

    if ((r_hi == 7 && r_lo == 5) && (g_hi == 4 && g_lo == 2) && (b_hi == 1 && b_lo == 0)) {
        return UNIBOOT_PIXEL_FORMAT_RGB_332;
    }

    if ((r_hi == 15 && r_lo == 11) && (g_hi == 10 && g_lo == 5) && (b_hi == 4 && b_lo == 0)) {
        return UNIBOOT_PIXEL_FORMAT_RGB_565;
    }

    if ((r_hi == 7 && r_lo == 6) && (g_hi == 5 && g_lo == 4) && (b_hi == 3 && b_lo == 2)) {
        return UNIBOOT_PIXEL_FORMAT_RGB_2220;
    }

unknown:
    printf("unknown pixel format bitmask: r %08x / g %08x / b %08x\n", bitmask.RedMask, bitmask.GreenMask, bitmask.BlueMask);
    return UNIBOOT_PIXEL_FORMAT_UNKNOWN;
}

static uint32_t fb_pixel_format(efi_graphics_output_protocol *gop) {
    efi_graphics_pixel_format efi_fmt = gop->Mode->Info->PixelFormat;
    switch (efi_fmt) {
    case PixelBlueGreenRedReserved8BitPerColor:
        return UNIBOOT_PIXEL_FORMAT_RGB_x888;
    case PixelBitMask:
        return fb_pixel_format_from_bitmask(gop->Mode->Info->PixelInformation);
    default:
        printf("unknown pixel format: %d\n", efi_fmt);
        return UNIBOOT_PIXEL_FORMAT_UNKNOWN;
    }
}

void read_framebuffer_info(void) {
    efi_graphics_output_protocol *gop = NULL;
    gBS->LocateProtocol(&GraphicsOutputProtocol, NULL, (void **)&gop);
    if (gop) {
        struct uniboot_entry *entry = bootinfo_alloc(struct uniboot_entry);
        entry->type = UNIBOOT_ENTRY_FRAMEBUFFER;
        entry->length = sizeof(struct uniboot_framebuffer);

        struct uniboot_framebuffer *fb = bootinfo_alloc(struct uniboot_framebuffer);
        fb->base = gop->Mode->FrameBufferBase;
        fb->width = gop->Mode->Info->HorizontalResolution;
        fb->height = gop->Mode->Info->VerticalResolution;
        fb->stride = gop->Mode->Info->PixelsPerScanLine;
        fb->format = fb_pixel_format(gop);
    }
}
