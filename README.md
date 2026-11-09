# LolOS 💻

A custom, bare-metal 32-bit operating system built from scratch.

## Overview
LolOS is a lightweight, monolithic testbed operating system operating in 32-bit Protected Mode. It features custom hardware drivers, a modular interactive shell, and direct hardware-to-memory communication without relying on standard C libraries or underlying host OS functions.

## Features
* **Core Architecture:** Multiboot compliant with a custom 32-bit bootloader and dedicated stack.
* **VGA Graphics:** Custom text-mode driver with dynamic, persistent screen theming.
* **Input/Output:** Non-blocking PS/2 keyboard driver utilizing raw CPU port mapping.
* **Interactive Shell:** Modular Ring 0 user environment featuring a ring-buffer command history (Up/Down arrow navigation).
* **Hardware Interfacing:** * Direct CMOS Real-Time Clock (RTC) querying for live time/date.
  * Inline Assembly execution (CPUID) for physical processor identification.
* **Persistent Storage:** ATA PIO block device driver capable of reading/writing raw 512-byte sectors to a virtual hard drive.
* **DevOps:** Automated CI/CD pipeline enforcing strict GCC compilation standards (`-Werror`).

## Security and next steps

This is an experimental OS, not a secure general-purpose system. Ring-3
execution, system calls, paging, storage, and filesystem support are prototypes
and must not be treated as security boundaries yet. See [SECURITY.md](SECURITY.md)
for the current security goals, known gaps, and migration checkpoints.

The `x86_64-iso` target builds a separate Multiboot 2 image that enters long
mode and prints a serial checkpoint. It deliberately does not reuse the
32-bit kernel yet. Its initial 1 GiB identity map is temporary bootstrap
plumbing; it is supervisor-only, but currently writable and executable. The
compiler disables builtins, PIE/PIC, stack-protector instrumentation without a
kernel runtime, unwind metadata, and the x86_64 red zone. These settings make
freestanding assumptions explicit; they do not replace bounds checks,
privilege checks, memory protection, or code review.

## Prerequisites
To build and run LolOs locally, you need the following installed on a Linux environment (Ubuntu recommended):
* `build-essential` (GCC, Make)
* `nasm` (Assembler)
* `qemu-system-x86` (Emulator for testing)
* **ISO Generation Tools**: `xorriso, grub-pc-bin, grub-common, mtools`

You can install these on Ubuntu via `sudo apt install build-essential nasm qemu-system-x86 xorriso grub-pc-bin grub-common mtools`

## Build and Run (for devs and contributing)
Clone the repository and use the included Makefile:

1. Compile the OS and launch it in QEMU:
   ```bash
   make run
   ```
2. Build and launch the parallel x86_64 long-mode checkpoint:
   ```bash
   make x86_64-iso
   make run-x86_64
   ```
3. Clean up build artifacts after finishing up (Note: `disk.img` will be deleted, wiping virtual disk data) :
   ```bash
   make clean
   ```

## Or if you're just curious (no compilation whatsoever required)
1. Go to the **Actions** tab at the top of this GitHub repository.
2. Click on the latest successful workflow run.
3. Scroll down to the **Artifacts** section and download the `.zip` file containing `myos.iso`.
4. Extract the file and boot it using QEMU:
   ```bash
   qemu-system-i386 -cdrom myos.iso
   ```
