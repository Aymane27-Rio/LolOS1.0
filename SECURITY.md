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
2. Add exception reporting, a 64-bit GDT/TSS and IDT, and dedicated exception
   stacks so CPU faults can be diagnosed without cascading into a triple fault.
3. Build a runtime identity map from normalized usable ranges and the kernel
   image. Keep mappings supervisor-only, add stack guard pages, and enforce
   W^X before enabling user mode. The first-GiB checkpoint implementation is
   in place; broader physical and virtual address layouts remain future work.
4. Add a syscall entry path with a documented ABI and safe privilege-transition
   stacks before enabling user mode.
5. Port allocation and ELF loading only after range validation, page ownership,
   user address limits, and permission handling are specified and reviewed.
6. Add malformed-input tests, emulator boot coverage, and targeted fuzzing for
   parsers and syscall boundaries before describing isolation as reliable.

## Known gaps

The existing 32-bit paging and ELF loader are experimental and currently lack
the invariants above. The x86_64 bootstrap uses a temporary first-GiB identity
map only to initialize the kernel. After normalizing the Multiboot memory map
and starting the physical page allocator, the kernel builds and activates a
new 4 KiB identity map for the kernel image and usable pages below 1 GiB. Kernel
text is read-only and executable, kernel data is writable and NX, and usable
RAM is supervisor writable and NX. The Multiboot header and read-only kernel
data are read-only and NX. Low memory, reserved ranges, boot modules,
boot-info memory, and four stack guard pages are left unmapped. The bootstrap
stack and the double-fault, NMI, and machine-check IST stacks each have one
unmapped guard page.
The checkpoint validates and retains up to 256 raw Multiboot 2 memory-map
entries. It derives page-aligned usable ranges after merging overlaps and
excluding non-usable entries, the first MiB, the kernel image, and the boot-info
block. Multiboot module tags are bounds-checked and each module's physical
range is excluded as well. A bitmap physical page allocator consumes those
ranges below the first-GiB mapping limit and zeros pages before returning them.
It is currently single-CPU and has no interrupt or SMP locking. The boot-info
pointer must fall within the bootstrap map while it is parsed; boot-info and
module pages are unmapped afterward. The x86_64 checkpoint installs handlers
for processor exception vectors 0 through 31, logs the exception frame and
page-fault address, and uses TSS-backed IST stacks for double fault, NMI, and
machine check. QEMU smoke tests check unmapped low memory and guard pages, write
protection on kernel text, and NX protection on writable data. The handlers
halt rather than recover. This checkpoint has no mapping for physical memory
above 1 GiB, MMIO/device windows, dynamic mappings, user mode, external
interrupt handling, filesystems, or a security boundary.
