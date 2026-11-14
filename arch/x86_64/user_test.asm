BITS 64

section .user_test_text
global x64_user_test_entry
x64_user_test_entry:
    ; Exercise a user-writable stack mapping before making any syscalls.
    mov rax, 0x5A17
    push rax
    pop rax
    cmp rax, 0x5A17
    jne .failed

    ; Unknown syscall must return -ENOSYS without damaging the user context.
    mov eax, 0xFFFF
    syscall
    cmp rax, -38
    jne .failed

    ; Reject unsupported descriptors without following the supplied pointer.
    mov eax, 1
    mov edi, 2
    mov esi, 0x80000
    mov edx, 8
    syscall
    cmp rax, -9
    jne .failed

    ; Enforce the write bound before examining the invalid user pointer.
    mov eax, 1
    mov edi, 1
    mov esi, 0x80000
    mov edx, 129
    syscall
    cmp rax, -7
    jne .failed

    ; An unmapped user pointer must be rejected before the kernel reads it.
    mov eax, 1
    mov edi, 1
    mov esi, 0x80000
    mov edx, 8
    syscall
    cmp rax, -14
    jne .failed

    ; A zero-length write must not inspect or dereference its pointer.
    mov eax, 1
    mov edi, 1
    mov esi, 0x80000
    xor edx, edx
    syscall
    test rax, rax
    jne .failed

    ; Write a bounded string from the read-only user data page.
    mov eax, 1
    mov edi, 1
    mov esi, __x64_user_test_message
    mov edx, __x64_user_test_message_end - __x64_user_test_message
    syscall
    cmp rax, __x64_user_test_message_end - __x64_user_test_message
    jne .failed

    mov eax, 0
    mov edi, 42
    syscall
    ud2

.failed:
    mov eax, 0
    mov edi, 255
    syscall
    ud2

section .user_test_rodata
global __x64_user_test_message
global __x64_user_test_message_end
__x64_user_test_message:
    db "Ring-3 syscall checks passed; bounded write works; user stack is writable.", 13, 10
__x64_user_test_message_end:

section .note.GNU-stack noalloc noexec nowrite progbits
