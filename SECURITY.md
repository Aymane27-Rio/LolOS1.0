# Security goals and current status

LolOS is a learning project. It is not suitable for sensitive data or as a
general-purpose operating system. Security work is an explicit design goal,
but features are not considered secure merely because they exist.

## Scope

The near-term threat model treats user programs, ELF files, filesystem images,
boot modules, and syscall arguments as untrusted input. The bootloader and
hardware are trusted for now. Secure Boot, hostile DMA devices, networking,
cryptographic identity, and multi-user policy are outside the current scope.

## Design invariants (required before trusting user code)

- Kernel code, data, stacks, page tables, and boot metadata are supervisor-only.
- User mappings are explicit, range-checked, minimally privileged, and cannot
  overwrite kernel or device memory.
- Writable memory is non-executable where supported; executable memory is not
  writable. Any temporary exception must be narrow and documented.
- Every length, offset, address range, and integer operation derived from
  external input is checked before access or allocation.
- Syscalls validate user pointers and copy data through checked interfaces;
  kernel code does not dereference untrusted pointers directly.
- Freed and newly allocated user memory is cleared. Exhaustion and malformed
  input fail closed instead of wrapping, aliasing, or continuing with null
  allocations.
- Interrupt and exception entry paths establish known CPU state and do not
  return to user mode with unchecked state.

## Migration checkpoints

1. Keep the current 32-bit system as a reference while building a separate
   Multiboot 2 x86_64 image. The initial image only verifies the transition to
   long mode and uses serial output.
2. Replace the temporary identity map with a physical memory map based layout,
   supervisor-only kernel mappings, guard pages, and W^X permissions before
   enabling user mode.
3. Add exception reporting, a 64-bit GDT/TSS and IDT, safe interrupt stacks,
   and a syscall entry path with a documented ABI.
4. Port allocation and ELF loading only after range validation, page ownership,
   user address limits, and permission handling are specified and reviewed.
5. Add malformed-input tests, emulator boot coverage, and targeted fuzzing for
   parsers and syscall boundaries before describing isolation as reliable.

## Known gaps

The existing 32-bit paging and ELF loader are experimental and currently lack
the invariants above. The x86_64 bootstrap maps the first GiB with 2 MiB pages;
those entries are supervisor-only, but writable and executable. That map is
only for reaching the serial checkpoint and must be replaced before porting
applications or device drivers. The x86_64 checkpoint validates the Multiboot 2
handoff and memory-map tag, but does not yet use those ranges to allocate memory
or construct mappings; its boot-info pointer must also fall within the currently
mapped first GiB. The x86_64 checkpoint does not yet provide user mode,
interrupts, memory allocation, filesystems, or a security boundary.
