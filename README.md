# LolOS 💻

A bare-metal operating system project with an experimental 32-bit reference
system and a separate x86_64 migration checkpoint.

## Overview
The original LolOS kernel is a 32-bit protected-mode prototype with an
interactive shell and experimental drivers. The separate x86_64 image is an
early boot checkpoint: it enters long mode, validates the Multiboot 2 memory
map, prints a short status over COM1, and halts. It does not yet run the shell
or reuse the 32-bit kernel.

## Features
* **Core Architecture:** Multiboot compliant with a custom 32-bit bootloader and dedicated stack.
* **VGA Graphics:** Custom text-mode driver with dynamic, persistent screen theming.
* **Input/Output:** Non-blocking PS/2 keyboard driver utilizing raw CPU port mapping.
* **Interactive Shell:** Experimental 32-bit kernel shell with ring-buffer command history (Up/Down arrow navigation).
* **Hardware Interfacing:** * Direct CMOS Real-Time Clock (RTC) querying for live time/date.
  * Inline Assembly execution (CPUID) for physical processor identification.
* **Persistent Storage:** ATA PIO block device driver capable of reading/writing raw 512-byte sectors to a virtual hard drive.
* **x86_64 migration:** Separate Multiboot 2 long-mode checkpoint with serial confirmation that the memory map validates.
* **Build pipeline:** GitHub Actions builds both images and prepares named ISO assets with SHA-256 checksums.

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
* `build-essential` and `gcc-multilib` (GCC, Make, 32-bit compiler support)
* `nasm` (Assembler)
* `qemu-system-x86` (Emulator for testing)
* **ISO Generation Tools**: `xorriso, grub-pc-bin, grub-common, mtools`

You can install these on Ubuntu via `sudo apt install build-essential gcc-multilib nasm qemu-system-x86 xorriso grub-pc-bin grub-common mtools`

## Build and Run
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
3. Clean up build artifacts after finishing up (Note: this deletes `disk.img` and any packaged assets in `dist/`):
   ```bash
   make clean
   ```

## Prepare release images locally

Build the two ISOs and their checksums with:

```bash
make release-assets
```

The files appear in `dist/`:

- `lolos-32bit-reference.iso` — the experimental original system.
- `lolos-x86_64-checkpoint.iso` — serial-only long-mode migration checkpoint.
- `SHA256SUMS.txt` — SHA-256 checksums for both images.

Verify them with `cd dist && sha256sum -c SHA256SUMS.txt`.

## Download a pre-release

When a prerelease tag such as `v0.0.1-pre.1` is pushed, GitHub Actions builds
both images, boots the x86_64 checkpoint in QEMU, packages the images with
checksums, and publishes a GitHub prerelease. Download the ISO assets from the
repository's **Releases** page. Workflow artifacts are temporary build outputs;
use the release assets for persistent downloads.

For the x86_64 image, run:

```bash
qemu-system-x86_64 -cdrom lolos-x86_64-checkpoint.iso -serial stdio -display none
```

For the 32-bit reference image, run:

```bash
qemu-system-i386 -cdrom lolos-32bit-reference.iso
```

Both images are experimental. The x86_64 image only verifies the boot
transition and reports memory ranges; neither image should be treated as a
secure or general-purpose operating system. See [SECURITY.md](SECURITY.md) and
the notes attached to each prerelease. The maintainer release steps are in
[RELEASING.md](RELEASING.md).
