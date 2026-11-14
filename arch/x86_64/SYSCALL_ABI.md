# LolOS x86_64 syscall ABI (prototype)

This ABI is an experimental bring-up interface for one fixed ring-3 test
program. It does not define process creation, scheduling, signals, file
descriptors beyond the test console, or a stable application binary interface.
The kernel remains a single-CPU ring-0 prototype.

## Entry and register convention

The CPU's `SYSCALL` instruction loads the kernel code/data selectors from
`IA32_STAR`, saves the user return RIP in `RCX`, saves user flags in `R11`, and
does not switch stacks. The entry stub swaps GS, saves the user stack pointer,
and switches to a dedicated guarded kernel stack before calling C code. TSS
`RSP0` points to that same stack for exceptions from ring 3.

The syscall number and arguments are:

| Register | Meaning |
| --- | --- |
| `RAX` | syscall number |
| `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9` | arguments 0 through 5 |
| `RAX` on return | result or negative error number |

The current entry preserves all general registers except `RAX`, `RCX`, and
`R11`. `RCX` and `R11` are clobbered by the instruction and return path. User
flags are sanitized before `SYSRET`; IF, TF, DF, IOPL, NT, and AC are not
restored. User mode starts with interrupts disabled. The fixed test does not
enable external interrupts.

## Implemented calls

| Number | Name | Arguments | Result |
| --- | --- | --- | --- |
| 0 | `exit` | `RDI`: status | Returns to the kernel test harness; this is not a general process exit. |
| 1 | `write` | `RDI`: descriptor, `RSI`: user buffer, `RDX`: length | Number of bytes written, or a negative error. Descriptor 1 writes to COM1. |

`write` accepts at most 128 bytes per call. The kernel checks the address range
for overflow and verifies every covered page is present and user-readable before
copying it into a bounded kernel buffer. Errors are `-EBADF` (-9) for an
unsupported descriptor, `-E2BIG` (-7) for an oversized write, `-EFAULT` (-14)
for an invalid user range, and `-ENOSYS` (-38) for an unknown call. A zero-byte
write to descriptor 1 returns zero without reading the supplied pointer.

## Current user mapping

The built-in probe has separate read-only executable text and read-only NX data
pages. Its one writable NX stack page is allocated from the physical page
allocator at virtual address `0x40002000`; the preceding page at `0x40001000`
is absent as a guard. The probe performs a stack push/pop and the harness
checks that the stack is user-writable while the guard remains unmapped. Other
kernel leaves remain supervisor-only even though the paging-structure parents
covering the probe have the user bit set.

The syscall return path accepts only a RIP inside the fixed test-text section
and an RSP within the fixed stack page. The test exercises stack access, an
unknown syscall, unsupported descriptor, oversized and zero-length writes, an
unmapped pointer, a valid bounded write, and `exit`. No arbitrary ELF or
user-supplied code can be loaded yet. The ABI, memory layout, and syscall
semantics may change as process and address-space support is developed.
