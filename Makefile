CC = gcc

CFLAGS = -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Werror
LDFLAGS = -m elf_i386 -T linker.ld -z noexecstack
AS = nasm
ASFLAGS = -f elf32

OBJS = boot/boot.o kernel/kernel.o kernel/keyboard.o kernel/terminal.o kernel/string.o kernel/cmos.o kernel/shell.o kernel/cpu.o kernel/ata.o kernel/fs.o kernel/graphics.o kernel/font.o kernel/mouse.o kernel/pmm.o kernel/paging.o kernel/idt.o kernel/pic.o kernel/timer.o kernel/gdt.o kernel/syscall.o kernel/elf.o kernel/vfs.o

all: myos.iso disk.img app.elf

app.elf: user/app.c
	gcc -m32 -ffreestanding -nostdlib -fno-pie -e _start -o app.elf user/app.c

myos.bin: $(OBJS)
		ld $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

# bootable ISO using GRUB
myos.iso: myos.bin app.elf
	mkdir -p isodir/boot/grub
	cp myos.bin isodir/boot/
	cp app.elf isodir/boot/
	echo 'menuentry "LolOS" { multiboot /boot/myos.bin; module /boot/app.elf; boot }' > isodir/boot/grub/grub.cfg
	grub-mkrescue -o myos.iso isodir

disk.img:
	dd if=/dev/zero of=disk.img bs=512 count=2048

run: myos.iso disk.img
	qemu-system-i386 -cdrom myos.iso -drive file=disk.img,format=raw,index=0,media=disk

clean:
	rm -rf $(OBJS) myos.bin myos.iso isodir disk.img