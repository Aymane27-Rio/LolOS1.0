This is an experimental LolOS pre-release for sharing the current x86_64
migration checkpoint. It is a development milestone, not a general-purpose or
secure operating system.

## Downloadable images

- `lolos-x86_64-checkpoint.iso` boots through Multiboot 2, enters x86_64 long
  mode, validates and prints the usable memory ranges over COM1, then halts.
  It has no interactive shell, user mode, or device drivers.
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
qemu-system-x86_64 -cdrom lolos-x86_64-checkpoint.iso -serial stdio -display none
```

The checkpoint uses a temporary writable and executable identity map over the
first GiB. Do not use either image with sensitive data or treat it as an
isolated or secure operating system. See `SECURITY.md` for the current gaps.
