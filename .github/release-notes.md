This is an experimental LolOS pre-release for sharing the current x86_64
migration checkpoint. It is a development milestone, not a general-purpose or
secure operating system.

## Downloadable images

- `lolos-x86_64-checkpoint.iso` boots through Multiboot 2, enters x86_64 long
  mode, validates and retains raw memory-map entries, derives page-aligned
  usable ranges after reservations, and provides a zeroing page allocator for
  the currently identity-mapped first GiB. Use `help`, `info`, `memmap`,
  `memmap usable`, `modules`, `pmm`, and `pmm selftest` to explore it. The
  allocator is single-CPU only and is not used to construct page tables. The
  checkpoint has no user mode, interrupts, general device drivers, or network
  stack.
- `lolos-32bit-reference.iso` is the separate experimental 32-bit reference
  system. Its paging, ELF, syscall, filesystem, and storage paths are
  prototypes and are not security boundaries.
- `SHA256SUMS.txt` contains checksums for both ISO files.

Verify downloaded images from the directory containing all three assets:

```sh
sha256sum -c SHA256SUMS.txt
```

Boot the x86_64 checkpoint in QEMU and view its serial output:

```sh
qemu-system-x86_64 -cdrom lolos-x86_64-checkpoint.iso -serial stdio -monitor none -display none
```

The checkpoint uses a temporary first-GiB identity map. Kernel text is
read-only and executable, read-only data is NX, and the remaining mapped
memory is writable and NX. The command prompt runs in ring 0 and provides no
user isolation. Do not use either image with sensitive data or treat it as a
secure operating system. See `SECURITY.md` for the current gaps.
