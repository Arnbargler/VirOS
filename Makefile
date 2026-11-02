# GNU-EFI build for x86_64 UEFI on Debian/Ubuntu.
ARCH := x86_64
CC := gcc
LD := ld
OBJCOPY := objcopy
EFI_INC := /usr/include/efi
EFI_LIB := /usr/lib
EFI_CRT := $(firstword $(wildcard /usr/lib/crt0-efi-x86_64.o /usr/lib/gnuefi/crt0-efi-x86_64.o))
EFI_LDS := $(firstword $(wildcard /usr/lib/elf_x86_64_efi.lds /usr/lib/gnuefi/elf_x86_64_efi.lds))
EFI_LIBDIR := $(if $(wildcard /usr/lib/libgnuefi.a),/usr/lib,/usr/lib/gnuefi)
CFLAGS := -I$(EFI_INC) -I$(EFI_INC)/$(ARCH) -fpic -ffreestanding -fno-stack-protector -fshort-wchar -mno-red-zone -Wall -Wextra -DEFI_FUNCTION_WRAPPER
LDFLAGS := -nostdlib -znocombreloc -T $(EFI_LDS) -shared -Bsymbolic -L$(EFI_LIBDIR) -L/usr/lib
OUT := dist/EFI/BOOT/BOOTX64.EFI

.PHONY: all clean usb
all: $(OUT) dist/APPS/BLOCKS.EFI dist/APPS/CHESS.EFI

build/launcher.o: src/launcher.c
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/launcher.so: build/launcher.o
	$(LD) $(LDFLAGS) $(EFI_CRT) $< -o $@ -lefi -lgnuefi

$(OUT): build/launcher.so
	mkdir -p dist/EFI/BOOT dist/APPS
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc --target=efi-app-$(ARCH) $< $@

clean:
	rm -rf build dist

# Copy dist/ contents to an already mounted, FAT32-formatted USB stick.
# Example: make usb USB=/media/$$(whoami)/VIROS
usb: all
	test -n "$(USB)" && test -d "$(USB)"
	cp -a dist/. "$(USB)/"
	sync


build/blocks.o: apps/blocks/main.c
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/blocks.so: build/blocks.o
	$(LD) $(LDFLAGS) $(EFI_CRT) $< -o $@ -lefi -lgnuefi

dist/APPS/BLOCKS.EFI: build/blocks.so
	mkdir -p dist/APPS
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc --target=efi-app-$(ARCH) $< $@


build/chess.o: apps/chess/main.c
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/chess.so: build/chess.o
	$(LD) $(LDFLAGS) $(EFI_CRT) $< -o $@ -lefi -lgnuefi

dist/APPS/CHESS.EFI: build/chess.so
	mkdir -p dist/APPS
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc --target=efi-app-$(ARCH) $< $@
