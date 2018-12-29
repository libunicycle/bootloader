ARCH = x86_64

LD=$(TOOLCHAIN_PATH)ld
NM=$(TOOLCHAIN_PATH)nm
OBJCOPY=$(TOOLCHAIN_PATH)objcopy
CC?=$(TOOLCHAIN_PATH)gcc

CFLAGS = -Ixefi -Iinclude -flto -W -Wall -Wextra -O3 -fno-stack-protector -fno-math-errno -fno-trapping-math -fno-common -fshort-wchar -nostdlib -ffreestanding -std=c11 -m64
ifeq ($(ARCH),x86_64)
CFLAGS += -mno-red-zone
endif

EFI_SECTIONS = -j .text -j .data -j .reloc

ifeq ($(CC),clang)
  CFLAGS += --target=$(ARCH)-windows-msvc
  LDFLAGS += /subsystem:efi_application /entry:efi_main
else
  CFLAGS += -fPIE
  LDFLAGS += -Wl,-T xefi/efi-x86-64.lds -Wl,--gc-sections
endif

TARGET = bootloader.efi

OBJS = bootloader.o \
       config.o \
       netboot.o \
       loadelf.o \
       bootinfo.o \
       framebuffer.o \
       inet.o \
       inet6.o \
       netifc.o \
       device_id.o \
       tftp/tftp.o \
       xefi/xefi.o \
       xefi/guids.o \
       xefi/printf.o \
       xefi/console-printf.o \
       xefi/ctype.o \
       xefi/string.o \
       xefi/strings.o \
       xefi/stdlib.o \
       xefi/loadfile.o

all: $(TARGET)

ifeq ($(CC),clang)

bootloader.efi: $(OBJS)
	lld-link $(LDFLAGS) $^ /out:$@
	if [ "`$(NM) $< | grep ' U '`" != "" ]; then echo "error: $<: undefined symbols"; $(NM) $< | grep ' U '; rm $<; exit 1; fi

else

bootloader.so: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

%.efi: %.so
	$(OBJCOPY) $(EFI_SECTIONS) --target=efi-app-$(ARCH) $^ $@
	if [ "`$(NM) $< | grep ' U '`" != "" ]; then echo "error: $<: undefined symbols"; $(NM) $< | grep ' U '; rm $<; exit 1; fi

endif
