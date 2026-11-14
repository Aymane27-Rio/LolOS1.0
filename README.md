# LolOS 💻

A bare-metal operating system project with an experimental 32-bit reference
system and a separate x86_64 migration checkpoint.

## Overview
The original LolOS kernel is a 32-bit protected-mode prototype with an
interactive shell and experimental drivers. The separate x86_64 image enters
long mode, validates and retains the Multiboot 2 memory map, then provides a
minimal command prompt over polled COM1 serial. It is a separate kernel and
does not reuse the 32-bit shell or drivers.

## Features
* **Core Architecture:** Multiboot compliant with a custom 32-bit bootloader and dedicated stack.
* **VGA Graphics:** Custom text-mode driver with dynamic, persistent screen theming.
* **Input/Output:** Non-blocking PS/2 keyboard driver utilizing raw CPU port mapping.
* **Interactive Shell:** Experimental 32-bit kernel shell with ring-buffer command history (Up/Down arrow navigation).
* **Hardware Interfacing:** * Direct CMOS Real-Time Clock (RTC) querying for live time/date.
  * Inline Assembly execution (CPUID) for physical processor identification.
* **Persistent Storage:** ATA PIO block device driver capable of reading/writing raw 512-byte sectors to a virtual hard drive.
* **x86_64 migration:** Separate Multiboot 2 kernel with a serial command shell, normalized memory-map inspection, a zeroing physical page allocator, memory-map-based supervisor page tables, stack guards, and fatal processor-exception reports.
* **Build pipeline:** GitHub Actions builds both images and prepares named ISO assets with SHA-256 checksums.

## Security and next steps

This is an experimental OS, not a secure general-purpose system. Ring-3
execution, system calls, paging, storage, and filesystem support are prototypes
and must not be treated as security boundaries yet. See [SECURITY.md](SECURITY.md)
for the current security goals, known gaps, and migration checkpoints.

The `x86_64-iso` target builds a separate Multiboot 2 image with a polled COM1
command prompt. Type `help` for the available commands. The prompt supports
basic line editing, raw memory-map inspection with `memmap`, normalized
page-aligned ranges with `memmap usable`, module reservations with `modules`,
page-table statistics with `paging`, a fixed ring-3 syscall probe with
`user-test`, and page allocator status/self-test commands with `pmm` and
`pmm selftest`. The allocator returns pages below the
1 GiB identity-map limit, zeroes each page, and excludes the kernel, boot-info
block, low memory, non-usable ranges, and Multiboot modules. It is a single-CPU
bring-up allocator without interrupt or SMP locking. The x86_64 kernel
deliberately does not reuse the 32-bit kernel.

The bootstrap identity map is replaced after memory-map normalization by a
runtime 4 KiB identity map for the kernel image and usable RAM below 1 GiB.
Kernel text is read-only and executable; kernel data and usable RAM are
supervisor writable and NX. Read-only kernel data is supervisor read-only and
NX. Low memory, reserved ranges, boot modules, boot-info memory, and guard
pages are unmapped. The bootstrap stack and three exception IST stacks have
one-page guards. `exception-test lowmem|guard|ro|nx` deliberately triggers
page faults to exercise those protections. This layout does not yet map RAM
above 1 GiB, MMIO/device windows, dynamic regions, or user pages. The compiler
disables builtins, PIE/PIC,
stack-protector instrumentation without a kernel runtime, unwind metadata, and
the x86_64 red zone. These settings make freestanding assumptions explicit;
they do not replace bounds checks, privilege checks, memory protection, or code
review.

The experimental syscall ABI is documented in
[`arch/x86_64/SYSCALL_ABI.md`](arch/x86_64/SYSCALL_ABI.md). `user-test` enters a
fixed ring-3 probe that checks `write`, invalid pointers, unknown syscall
numbers, and `exit`; it does not load arbitrary programs or establish a general
security boundary.

The x86_64 kernel also installs handlers for processor exception vectors 0–31
and a TSS with dedicated guarded stacks for double fault, NMI, and machine
check. Fatal reports include the vector, error code, instruction pointer, and
page-fault address. The `exception-test` commands deliberately trigger a fatal
exception and halt the VM. The kernel does not yet handle external interrupts
or recover from exceptions.

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
   Use the terminal attached to QEMU's serial port. Type `help` to list
   commands such as `info`, `memmap`, `memmap usable`, `modules`, `paging`,
   `pmm`, `pmm selftest`, `exception-test`, `echo`, `clear`, and `halt`. The
   exception tests intentionally halt the kernel after reporting the fault.
   QEMU's video
   display is hidden because this checkpoint has no graphics console yet.
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
- `lolos-x86_64-checkpoint.iso` — x86_64 long-mode migration image with a serial command prompt.
- `SHA256SUMS.txt` — SHA-256 checksums for both images.

Verify them with `cd dist && sha256sum -c SHA256SUMS.txt`.

## Download a pre-release

When a matching pre-release tag is pushed, GitHub Actions builds both images,
boots the x86_64 command prompt in QEMU, packages the images with checksums,
and publishes a GitHub prerelease. Download the ISO assets from the repository's
**Releases** page. Workflow artifacts are temporary build outputs; use the
release assets for persistent downloads.

The published `v0.0.1-pre.1` x86_64 ISO predates the serial command prompt and
halts after its boot check. Build this checkout with `make x86_64-iso` to try
the interactive console.

For the x86_64 image, run:

```bash
qemu-system-x86_64 -cdrom lolos-x86_64-checkpoint.iso -serial stdio -display none
```

For the 32-bit reference image, run:

```bash
qemu-system-i386 -cdrom lolos-32bit-reference.iso
```

Both images are experimental. The x86_64 image is a ring-0 bring-up console;
its physical page allocator only serves the temporary identity-mapped first
GiB and is not yet used for page-table construction or user memory. Neither
image should be treated as a secure or
general-purpose operating system. See [SECURITY.md](SECURITY.md) and the notes
attached to each prerelease. The maintainer release steps are in
[RELEASING.md](RELEASING.md).
