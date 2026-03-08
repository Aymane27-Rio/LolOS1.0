CC = gcc

CFLAGS = -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Werror
LDFLAGS = -m elf_i386 -T linker.ld
AS = nasm
ASFLAGS = -f elf32

OBJS = boot/boot.o kernel/kernel.o kernel/keyboard.o kernel/terminal.o kernel/string.o kernel/cmos.o kernel/shell.o kernel/cpu.o kernel/ata.o kernel/fs.o

all: myos.bin disk.img

myos.bin: $(OBJS)
		ld $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

disk.img:
	dd if=/dev/zero of=disk.img bs=512 count=2048

run: myos.bin disk.img
	qemu-system-i386 -kernel myos.bin -drive file=disk.img,format=raw,index=0,media=disk

clean:
	rm -f $(OBJS) myos.bin