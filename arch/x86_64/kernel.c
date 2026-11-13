#include <stdint.h>
#include "console.h"
#include "exceptions.h"
#include "memory_map.h"
#include "page_table.h"
#include "pmm.h"
#include "serial.h"

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

struct mb2_module_tag {
    uint32_t type;
    uint32_t size;
    uint32_t module_start;
    uint32_t module_end;
} __attribute__((packed));

struct x64_memory_region
    x64_memory_map_entries[X64_MEMORY_MAP_MAX_ENTRIES];
uint32_t x64_memory_map_entry_count;

extern unsigned char __kernel_start[];
extern unsigned char __kernel_end[];

static int parse_memory_map(uint32_t boot_magic, uint32_t boot_info,
                            uint32_t *boot_info_size_out)
{
    const uint8_t *info = (const uint8_t *)(uintptr_t)boot_info;
    uint32_t total_size;
    uint32_t offset;
    int found_end = 0;
    int found_mmap = 0;
    int found_usable_memory = 0;

    x64_memory_map_entry_count = 0;
    x64_memory_map_reset_boot_modules();

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
            for (entry_offset = 0; entry_offset < payload_size;
                 entry_offset += mmap_tag->entry_size) {
                const struct mb2_mmap_entry *entry =
                    (const struct mb2_mmap_entry *)
                    ((const uint8_t *)mmap_tag + sizeof(*mmap_tag) + entry_offset);

                if (entry->length > UINT64_MAX - entry->address) {
                    serial_write("Multiboot 2: overflowing memory-map range\n");
                    return 0;
                }

                if (x64_memory_map_entry_count == X64_MEMORY_MAP_MAX_ENTRIES) {
                    serial_write("Multiboot 2: memory map exceeds entry capacity\n");
                    return 0;
                }
                x64_memory_map_entries[x64_memory_map_entry_count].base =
                    entry->address;
                x64_memory_map_entries[x64_memory_map_entry_count].length =
                    entry->length;
                x64_memory_map_entries[x64_memory_map_entry_count].type =
                    entry->type;
                x64_memory_map_entry_count++;

                if (entry->type == 1u && entry->length != 0u) {
                    found_usable_memory = 1;
                }
            }
        }

        if (tag->type == 3u) {
            const struct mb2_module_tag *module =
                (const struct mb2_module_tag *)tag;
            uint32_t string_offset;
            int has_string_terminator = 0;

            if (tag->size < sizeof(*module) + 1u ||
                module->module_end < module->module_start) {
                serial_write("Multiboot 2: invalid module tag\n");
                return 0;
            }
            for (string_offset = sizeof(*module);
                 string_offset < tag->size; string_offset++) {
                if (((const uint8_t *)tag)[string_offset] == 0u) {
                    has_string_terminator = 1;
                    break;
                }
            }
            if (!has_string_terminator ||
                !x64_memory_map_add_boot_module(module->module_start,
                                                module->module_end)) {
                serial_write("Multiboot 2: malformed or excessive module metadata\n");
                return 0;
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

    if (!found_end || !found_mmap || !found_usable_memory) {
        serial_write("Multiboot 2: missing end tag, memory map, or usable memory\n");
        return 0;
    }
    *boot_info_size_out = total_size;
    return 1;
}

void x64_kmain(uint32_t boot_magic, uint32_t boot_info)
{
    uint32_t boot_info_size;

    serial_init();
    x64_exception_init();
    serial_write("x86_64 exception handlers initialized (IDT/TSS/IST).\n");
    serial_write("LolOS x86_64: boot handoff received.\n");
    if (!parse_memory_map(boot_magic, boot_info, &boot_info_size)) {
        serial_write("Memory-map parsing failed; halting.\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }
    if (!x64_normalize_usable_ranges(
            boot_info, boot_info_size,
            (uint64_t)(uintptr_t)__kernel_start,
            (uint64_t)(uintptr_t)__kernel_end -
                (uint64_t)(uintptr_t)__kernel_start)) {
        serial_write("Memory-map normalization failed; halting.\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }
    serial_write("LolOS x86_64: memory map normalized.\n");
    serial_write("LolOS x86_64: initializing physical page allocator.\n");
    if (!x64_pmm_init()) {
        serial_write("Physical page allocator initialization failed; halting.\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }
    serial_write("LolOS x86_64: building memory-map-based page tables.\n");
    if (!x64_page_table_init()) {
        serial_write("Runtime page-table construction failed; halting.\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }
    serial_write("LolOS x86_64: runtime page-table layout active.\n");
    serial_write("LolOS x86_64: boot map valid.\n");
    x64_console_run();
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
