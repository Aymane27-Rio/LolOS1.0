CC = gcc

CFLAGS = -m32 -std=gnu99 -ffreestanding -fno-builtin -fno-pie -fno-pic \
	-fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables \
	-O2 -Wall -Wextra -Werror
LDFLAGS = -m elf_i386 -T linker.ld --build-id=none -z noexecstack
AS = nasm
ASFLAGS = -f elf32
QEMU_X86_64 = qemu-system-x86_64
X64_DIR = build/x86_64
X64_OBJS = $(X64_DIR)/boot.o $(X64_DIR)/kernel.o $(X64_DIR)/memory_map.o \
	$(X64_DIR)/pmm.o $(X64_DIR)/page_table.o $(X64_DIR)/serial.o $(X64_DIR)/console.o \
	$(X64_DIR)/exceptions.o $(X64_DIR)/exception_stubs.o
X64_CFLAGS = -m64 -std=gnu11 -O2 -Wall -Wextra -Werror \
	-ffreestanding -fno-builtin -fno-pie -fno-pic -fno-stack-protector \
	-fno-asynchronous-unwind-tables -fno-unwind-tables -mno-red-zone \
	-mgeneral-regs-only -mcmodel=small -march=x86-64 -mtune=generic

OBJS = boot/boot.o kernel/kernel.o kernel/keyboard.o kernel/terminal.o kernel/string.o kernel/cmos.o kernel/shell.o kernel/cpu.o kernel/ata.o kernel/fs.o kernel/graphics.o kernel/font.o kernel/mouse.o kernel/pmm.o kernel/paging.o kernel/idt.o kernel/pic.o kernel/timer.o kernel/gdt.o kernel/syscall.o kernel/vfs.o

.PHONY: all clean run x86_64-iso run-x86_64 x86_64-memory-test x86_64-module-test-iso release-assets

all: myos.iso disk.img

myos.bin: $(OBJS)
		ld $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

# bootable ISO using GRUB
myos.iso: myos.bin
	mkdir -p isodir/boot/grub
	cp myos.bin isodir/boot/
	echo 'menuentry "LolOS" { multiboot /boot/myos.bin; boot }' > isodir/boot/grub/grub.cfg
	grub-mkrescue -o myos.iso isodir

disk.img:
	dd if=/dev/zero of=disk.img bs=512 count=2048

run: myos.iso disk.img
	qemu-system-i386 -cdrom myos.iso -drive file=disk.img,format=raw,index=0,media=disk

# Parallel x86_64 long-mode bring-up; the existing 32-bit image stays the default.
x86_64-iso: $(X64_DIR)/lolos-x86_64.iso

$(X64_DIR):
	mkdir -p $@

$(X64_DIR)/boot.o: arch/x86_64/boot.asm | $(X64_DIR)
	$(AS) -f elf64 $< -o $@

$(X64_DIR)/kernel.o: arch/x86_64/kernel.c arch/x86_64/console.h arch/x86_64/exceptions.h arch/x86_64/memory_map.h arch/x86_64/page_table.h arch/x86_64/pmm.h arch/x86_64/serial.h | $(X64_DIR)
	$(CC) $(X64_CFLAGS) -c $< -o $@

$(X64_DIR)/memory_map.o: arch/x86_64/memory_map.c arch/x86_64/memory_map.h arch/x86_64/serial.h | $(X64_DIR)
	$(CC) $(X64_CFLAGS) -c $< -o $@

$(X64_DIR)/pmm.o: arch/x86_64/pmm.c arch/x86_64/pmm.h arch/x86_64/memory_map.h | $(X64_DIR)
	$(CC) $(X64_CFLAGS) -c $< -o $@

$(X64_DIR)/page_table.o: arch/x86_64/page_table.c arch/x86_64/page_table.h arch/x86_64/pmm.h arch/x86_64/memory_map.h | $(X64_DIR)
	$(CC) $(X64_CFLAGS) -c $< -o $@

$(X64_DIR)/serial.o: arch/x86_64/serial.c arch/x86_64/serial.h | $(X64_DIR)
	$(CC) $(X64_CFLAGS) -c $< -o $@

$(X64_DIR)/console.o: arch/x86_64/console.c arch/x86_64/console.h arch/x86_64/memory_map.h arch/x86_64/page_table.h arch/x86_64/pmm.h arch/x86_64/serial.h | $(X64_DIR)
	$(CC) $(X64_CFLAGS) -c $< -o $@

$(X64_DIR)/exceptions.o: arch/x86_64/exceptions.c arch/x86_64/exceptions.h arch/x86_64/serial.h | $(X64_DIR)
	$(CC) $(X64_CFLAGS) -c $< -o $@

$(X64_DIR)/exception_stubs.o: arch/x86_64/exception_stubs.asm | $(X64_DIR)
	$(AS) -f elf64 $< -o $@

$(X64_DIR)/memory_map_test: tests/x86_64_memory_map_test.c arch/x86_64/memory_map.c arch/x86_64/memory_map.h arch/x86_64/serial.h | $(X64_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iarch/x86_64 tests/x86_64_memory_map_test.c arch/x86_64/memory_map.c -o $@

x86_64-memory-test: $(X64_DIR)/memory_map_test
	$(X64_DIR)/memory_map_test

$(X64_DIR)/lolos-x86_64-module-test.iso: $(X64_DIR)/lolos-x86_64.bin tests/x86_64_test_module.txt | $(X64_DIR)
	mkdir -p $(X64_DIR)/module-test-isodir/boot/grub
	cp $(X64_DIR)/lolos-x86_64.bin $(X64_DIR)/module-test-isodir/boot/
	cp tests/x86_64_test_module.txt $(X64_DIR)/module-test-isodir/boot/
	printf '%s\n' 'set timeout=0' 'set default=0' 'menuentry "LolOS x86_64 module reservation test" {' '    multiboot2 /boot/lolos-x86_64.bin' '    module2 /boot/x86_64_test_module.txt module-reservation-test' '    boot' '}' > $(X64_DIR)/module-test-isodir/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(X64_DIR)/module-test-isodir

x86_64-module-test-iso: $(X64_DIR)/lolos-x86_64-module-test.iso

$(X64_DIR)/lolos-x86_64.bin: $(X64_OBJS) arch/x86_64/linker.ld
	$(LD) -m elf_x86_64 -T arch/x86_64/linker.ld --build-id=none -z noexecstack $(X64_OBJS) -o $@

$(X64_DIR)/lolos-x86_64.iso: $(X64_DIR)/lolos-x86_64.bin arch/x86_64/grub.cfg
	mkdir -p $(X64_DIR)/isodir/boot/grub
	cp $(X64_DIR)/lolos-x86_64.bin $(X64_DIR)/isodir/boot/
	cp arch/x86_64/grub.cfg $(X64_DIR)/isodir/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(X64_DIR)/isodir

run-x86_64: $(X64_DIR)/lolos-x86_64.iso
	$(QEMU_X86_64) -cdrom $< -serial stdio -monitor none -display none -no-reboot -no-shutdown

# Create the named ISO assets and checksums used by GitHub prereleases.
release-assets: myos.iso $(X64_DIR)/lolos-x86_64.iso
	mkdir -p dist
	cp myos.iso dist/lolos-32bit-reference.iso
	cp $(X64_DIR)/lolos-x86_64.iso dist/lolos-x86_64-checkpoint.iso
	cd dist && sha256sum lolos-32bit-reference.iso lolos-x86_64-checkpoint.iso > SHA256SUMS.txt

clean:
	rm -rf $(OBJS) myos.bin myos.iso isodir disk.img app.elf build dist
