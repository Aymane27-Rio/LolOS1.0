#include <stdint.h>

#define COM1 0x3F8u
#define MB2_BOOT_MAGIC 0x36D76289u
#define IDENTITY_MAP_SIZE 0x40000000u

struct mb2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct mb2_mmap_tag {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} __attribute__((packed));

struct mb2_mmap_entry {
    uint64_t address;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed));

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port) : "memory");
    return value;
}

static void serial_putc(char character)
{
    while ((inb(COM1 + 5) & 0x20u) == 0u) {
        __asm__ volatile ("pause");
    }
    outb(COM1, (uint8_t)character);
}

static void serial_write(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n') {
            serial_putc('\r');
        }
        serial_putc(*text++);
    }
}

static void serial_hex64(uint64_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    for (int shift = 60; shift >= 0; shift -= 4) {
        serial_putc(digits[(value >> shift) & 0x0Fu]);
    }
}

static int parse_memory_map(uint32_t boot_magic, uint32_t boot_info)
{
    const uint8_t *info = (const uint8_t *)(uintptr_t)boot_info;
    uint32_t total_size;
    uint32_t offset;
    int found_end = 0;
    int found_mmap = 0;
    uint32_t usable_entries = 0;

    if (boot_magic != MB2_BOOT_MAGIC) {
        serial_write("Multiboot 2: invalid boot magic\n");
        return 0;
    }
    if ((boot_info & 7u) != 0 || boot_info >= IDENTITY_MAP_SIZE ||
        IDENTITY_MAP_SIZE - boot_info < 8u) {
        serial_write("Multiboot 2: boot-info pointer is outside the mapped range\n");
        return 0;
    }

    total_size = *(const uint32_t *)info;
    if (total_size < 16u || (total_size & 7u) != 0 ||
        total_size > IDENTITY_MAP_SIZE - boot_info) {
        serial_write("Multiboot 2: invalid boot-info size\n");
        return 0;
    }

    offset = 8u;
    while (offset <= total_size - 8u) {
        const struct mb2_tag *tag = (const struct mb2_tag *)(info + offset);
        uint32_t remaining = total_size - offset;
        uint32_t next_offset;

        if (tag->size < 8u || tag->size > remaining) {
            serial_write("Multiboot 2: malformed tag bounds\n");
            return 0;
        }

        if (tag->type == 0u) {
            if (tag->size != 8u || tag->size != remaining) {
                serial_write("Multiboot 2: malformed end tag\n");
                return 0;
            }
            found_end = 1;
            break;
        }

        if (tag->type == 6u) {
            const struct mb2_mmap_tag *mmap_tag =
                (const struct mb2_mmap_tag *)tag;
            uint32_t payload_size;
            uint32_t entry_offset;

            if (found_mmap || tag->size < sizeof(*mmap_tag) ||
                mmap_tag->entry_version != 0u ||
                mmap_tag->entry_size < sizeof(struct mb2_mmap_entry) ||
                (mmap_tag->entry_size & 7u) != 0u) {
                serial_write("Multiboot 2: invalid memory-map tag\n");
                return 0;
            }
            payload_size = tag->size - (uint32_t)sizeof(*mmap_tag);
            if (payload_size % mmap_tag->entry_size != 0u) {
                serial_write("Multiboot 2: partial memory-map entry\n");
                return 0;
            }

            found_mmap = 1;
            serial_write("Usable physical memory ranges:\n");
            for (entry_offset = 0; entry_offset < payload_size;
                 entry_offset += mmap_tag->entry_size) {
                const struct mb2_mmap_entry *entry =
                    (const struct mb2_mmap_entry *)
                    ((const uint8_t *)mmap_tag + sizeof(*mmap_tag) + entry_offset);

                if (entry->length > UINT64_MAX - entry->address) {
                    serial_write("Multiboot 2: overflowing memory-map range\n");
                    return 0;
                }
                if (entry->type == 1u && entry->length != 0u) {
                    serial_write("  base=0x");
                    serial_hex64(entry->address);
                    serial_write(" length=0x");
                    serial_hex64(entry->length);
                    serial_write("\n");
                    usable_entries++;
                }
            }
        }

        /* total_size is bounded to the first GiB above, so rounding is safe. */
        next_offset = (offset + tag->size + 7u) & ~7u;
        if (next_offset <= offset || next_offset > total_size) {
            serial_write("Multiboot 2: tag alignment exceeds boot-info bounds\n");
            return 0;
        }
        offset = next_offset;
    }

    if (!found_end || !found_mmap || usable_entries == 0u) {
        serial_write("Multiboot 2: missing end tag, memory map, or usable memory\n");
        return 0;
    }
    return 1;
}

void x64_kmain(uint32_t boot_magic, uint32_t boot_info)
{
    serial_write("LolOS x86_64: long mode entered. Supervisor-only bootstrap map active.\n");
    if (!parse_memory_map(boot_magic, boot_info)) {
        serial_write("Memory-map parsing failed; halting.\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }
    serial_write("Memory-map metadata validated; bootstrap mapping remains unchanged.\n");
    serial_write("No user code or interrupts are enabled in this migration checkpoint.\n");

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
