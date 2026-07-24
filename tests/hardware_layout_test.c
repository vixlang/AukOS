#include <aukos/pci.h>
#include <aukos/virtio_blk.h>

#include <stdint.h>
#include <stdio.h>

#define HELLO_DATA_BLOCK 10u

static int expect(int condition)
{
    return condition ? 0 : 1;
}

int main(int argc, char **argv)
{
    uint32_t address = 0;
    uint16_t io_base = 0;
    uintptr_t memory_base = 0;
    size_t used_offset = 0;
    size_t total_size = 0;
    uint8_t image[VIRTIO_BLK_FIXTURE_SECTORS * 512u];
    FILE *fixture;
    int failures = 0;

    failures += expect(pci_make_config_address(0u, 3u, 2u, 0x10u, &address) == 0 &&
                       address == 0x80001a10u);
    failures += expect(pci_make_config_address(0u, 32u, 0u, 0u, &address) != 0);
    failures += expect(pci_make_config_address(0u, 0u, 0u, 2u, &address) != 0);
    failures += expect(pci_parse_io_bar(0x0000c001u, &io_base) == 0 && io_base == 0xc000u);
    failures += expect(pci_parse_io_bar(0xffffffffu, &io_base) != 0);
    failures += expect(pci_parse_io_bar(0x10000001u, &io_base) != 0);
    failures += expect(pci_parse_io_bar(0x0000c000u, &io_base) != 0);
    failures += expect(pci_parse_memory_bar32(0xfebc0000u, &memory_base) == 0 &&
                       memory_base == (uintptr_t)0xfebc0000u);
    failures += expect(pci_parse_memory_bar32(0x0000c001u, &memory_base) != 0);
    failures += expect(pci_parse_memory_bar32(0xffffffffu, &memory_base) != 0);
    failures += expect(virtio_legacy_queue_layout(128u, &used_offset, &total_size) == 0 &&
                       used_offset == 4096u && total_size == 5124u);
    failures += expect(virtio_legacy_queue_layout(0u, &used_offset, &total_size) != 0);
    failures += expect(virtio_legacy_queue_layout(256u, &used_offset, &total_size) == 0 &&
                       used_offset == 8192u && total_size == 10244u);
    failures += expect(virtio_legacy_queue_layout(257u, &used_offset, &total_size) != 0);

    if (argc != 2) {
        return 2;
    }
    fixture = fopen(argv[1], "rb");
    failures += expect(fixture != NULL);
    if (fixture) {
        failures += expect(fread(image, 1u, sizeof(image), fixture) == sizeof(image));
        failures += expect(fgetc(fixture) == EOF);
        fclose(fixture);
        failures += expect(image[1024u + 0x38u] == 0x53u &&
                           image[1024u + 0x39u] == 0xefu);
        failures += expect(image[HELLO_DATA_BLOCK * 1024u] == 'H');
        for (size_t index = (VIRTIO_BLK_FIXTURE_SECTORS - 1u) * 512u;
             index < sizeof(image); index++) {
            failures += expect(image[index] == 0u);
        }
    }
    return failures == 0 ? 0 : 1;
}
