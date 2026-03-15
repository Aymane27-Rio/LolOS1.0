void print(char* str) {
    __asm__ volatile (
        "mov $1, %%eax\n\t"    
        "mov %0, %%ebx\n\t"    
        "int $0x80\n\t"        
        : : "r" (str) : "eax", "ebx"
    );
}

void exit() {
    __asm__ volatile (
        "mov $2, %%eax\n\t"
        "int $0x80\n\t"
        : : : "eax"
    );
}

void _start() {
    print("\n\n  *** HELLO FROM A STANDALONE ELF EXECUTABLE! ***\n");
    print("  Running securely in Ring 3 User Space.\n");
    print("  Task complete. Requesting Kernel termination...\n");
    exit();
}