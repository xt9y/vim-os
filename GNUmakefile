# brew install x86_64-elf-gcc x86_64-elf-binutils xorriso qemu
# curl -Lo src/limine.h https://raw.githubusercontent.com/limine-bootloader/limine/v9.x-binary/limine.h

CC := x86_64-elf-gcc
LD := x86_64-elf-ld

SRC_DIRS := src src/kernel

CFILES    := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
OBJ       := $(patsubst %.c,obj/%.c.o,$(CFILES))
INCLUDES  := $(addprefix -I,$(SRC_DIRS))

CFLAGS  := -O2 -std=gnu11 -ffreestanding -fno-stack-protector -fno-PIC \
           -fno-lto -m64 -mno-red-zone -mno-sse -mno-sse2 -mgeneral-regs-only \
           -mcmodel=kernel -Wall -Wextra $(INCLUDES)
LDFLAGS := -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 -T linker.lds

OVMF := $(shell find /opt/homebrew /usr/local -name "edk2-x86_64-code.fd" 2>/dev/null | head -1)
SERIAL := stdio

.PHONY: all iso run clean

all: bin/os run

bin/os: $(OBJ) linker.lds GNUmakefile
	mkdir -p bin
	$(LD) $(LDFLAGS) $(OBJ) -o $@

obj/%.c.o: %.c GNUmakefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

iso: bin/os
	curl -Ls https://github.com/limine-bootloader/limine/releases/latest/download/limine-binary.tar.gz | tar -xz -C bin
	mkdir -p bin/iso_root/boot/limine bin/iso_root/EFI/BOOT
	cp bin/os bin/iso_root/boot/
	printf 'timeout: 0\n\n/myOS\n\tprotocol: limine\n\tpath: boot():/boot/os\n\tresolution: %dx%d\n' $(SCREEN_W) $(SCREEN_H) > bin/iso_root/boot/limine/limine.conf
	cp bin/limine-binary/limine-bios-cd.bin bin/iso_root/boot/limine/
	cp bin/limine-binary/limine-bios.sys    bin/iso_root/boot/limine/
	cp bin/limine-binary/limine-uefi-cd.bin bin/iso_root/boot/limine/
	cp bin/limine-binary/BOOTX64.EFI        bin/iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
	  -b boot/limine/limine-bios-cd.bin \
	  -no-emul-boot -boot-load-size 4 -boot-info-table \
	  --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part \
	  --efi-boot-image --protective-msdos-label \
	  bin/iso_root -o bin/image.iso

HOST_RES := $(shell system_profiler SPDisplaysDataType 2>/dev/null | grep Resolution | head -1 | awk '{print $$2,$$4}')
SCREEN_W := $(word 1,$(HOST_RES))
SCREEN_H := $(word 2,$(HOST_RES))
ifeq ($(SCREEN_W),)
SCREEN_W := 1280
SCREEN_H := 720
endif

run: iso
	qemu-system-x86_64 -cdrom bin/image.iso -m 128M \
	  -serial $(SERIAL) \
	  -drive if=pflash,format=raw,readonly=on,file=$(OVMF) \
	  -vga std

clean:
	rm -rf bin obj serial.log
