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
2. Clean up build artifacts after finishing up (Note: `disk.img` will be deleted, wiping virtual disk data) :
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