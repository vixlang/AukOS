#ifndef AUKOS_PCI_H
#define AUKOS_PCI_H

#include <stdint.h>

#define PCI_VENDOR_VIRTIO 0x1af4u
#define PCI_DEVICE_VIRTIO_NET_TRANSITIONAL 0x1000u
#define PCI_DEVICE_VIRTIO_BLK_TRANSITIONAL 0x1001u

struct pci_device {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t header_type;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t bar_count;
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t bars[6];
};

static inline int pci_make_config_address(uint8_t bus, uint8_t slot,
                                          uint8_t function, uint8_t offset,
                                          uint32_t *address)
{
    if (!address || slot >= 32u || function >= 8u || (offset & 3u) != 0) {
        return -1;
    }
    *address = 0x80000000u | ((uint32_t)bus << 16) |
               ((uint32_t)slot << 11) | ((uint32_t)function << 8) | offset;
    return 0;
}

static inline int pci_parse_io_bar(uint32_t raw_bar, uint16_t *io_base)
{
    uint32_t address;

    if (!io_base || raw_bar == 0xffffffffu || (raw_bar & 1u) == 0) {
        return -1;
    }
    address = raw_bar & ~3u;
    if (address == 0 || address > UINT16_MAX) {
        return -1;
    }
    *io_base = (uint16_t)address;
    return 0;
}

static inline int pci_parse_memory_bar32(uint32_t raw_bar,
                                         uintptr_t *physical_base)
{
    uintptr_t address;

    if (!physical_base || raw_bar == 0xffffffffu || (raw_bar & 1u) != 0u ||
        ((raw_bar >> 1) & 3u) != 0u) {
        return -1;
    }
    address = (uintptr_t)(raw_bar & ~0x0fu);
    if (address == 0u) {
        return -1;
    }
    *physical_base = address;
    return 0;
}

void pci_init(void);
const struct pci_device *pci_find_device(uint16_t vendor_id, uint16_t device_id);
const struct pci_device *pci_find_device_nth(uint16_t vendor_id,
                                             uint16_t device_id,
                                             uint16_t match_index);
int pci_get_io_bar(const struct pci_device *device, uint8_t index, uint16_t *io_base);
int pci_get_memory_bar32(const struct pci_device *device, uint8_t index,
                         uintptr_t *physical_base);
int pci_enable_io_bus_master(const struct pci_device *device);
int pci_enable_memory_bus_master(const struct pci_device *device);

#endif
