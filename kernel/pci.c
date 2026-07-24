#include "include/aukos/pci.h"

#include "include/aukos/io.h"
#include "include/aukos/log.h"

#include <stddef.h>
#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xcf8u
#define PCI_CONFIG_DATA 0xcfcu
#define PCI_MAX_DEVICES 256u
#define PCI_COMMAND_IO_SPACE 0x0001u
#define PCI_COMMAND_MEMORY_SPACE 0x0002u
#define PCI_COMMAND_BUS_MASTER 0x0004u

static struct pci_device devices[PCI_MAX_DEVICES];
static uint16_t device_count;
static int pci_ready;

static int config_read32(uint8_t bus, uint8_t slot, uint8_t function,
                         uint8_t offset, uint32_t *value)
{
    uint32_t address;

    if (!value || pci_make_config_address(bus, slot, function, offset, &address) != 0) {
        return -1;
    }
    outl(PCI_CONFIG_ADDRESS, address);
    *value = inl(PCI_CONFIG_DATA);
    return 0;
}

static int config_read16(uint8_t bus, uint8_t slot, uint8_t function,
                         uint8_t offset, uint16_t *value)
{
    uint32_t data;

    if (!value || (offset & 1u) != 0 ||
        config_read32(bus, slot, function, offset & 0xfcu, &data) != 0) {
        return -1;
    }
    *value = (uint16_t)(data >> ((offset & 2u) * 8u));
    return 0;
}

static int config_write16(uint8_t bus, uint8_t slot, uint8_t function,
                          uint8_t offset, uint16_t value)
{
    uint32_t address;

    if ((offset & 1u) != 0 ||
        pci_make_config_address(bus, slot, function, offset & 0xfcu, &address) != 0) {
        return -1;
    }
    outl(PCI_CONFIG_ADDRESS, address);
    outw((uint16_t)(PCI_CONFIG_DATA + (offset & 2u)), value);
    return 0;
}

static void record_function(uint8_t bus, uint8_t slot, uint8_t function)
{
    struct pci_device *device;
    uint32_t id;
    uint32_t class_info;
    uint32_t header_info;
    uint8_t header_layout;

    if (device_count >= PCI_MAX_DEVICES ||
        config_read32(bus, slot, function, 0x00u, &id) != 0 ||
        (uint16_t)id == 0xffffu || id == 0xffffffffu ||
        config_read32(bus, slot, function, 0x08u, &class_info) != 0 ||
        config_read32(bus, slot, function, 0x0cu, &header_info) != 0) {
        return;
    }

    device = &devices[device_count++];
    device->bus = bus;
    device->slot = slot;
    device->function = function;
    device->vendor_id = (uint16_t)id;
    device->device_id = (uint16_t)(id >> 16);
    device->prog_if = (uint8_t)(class_info >> 8);
    device->subclass = (uint8_t)(class_info >> 16);
    device->class_code = (uint8_t)(class_info >> 24);
    device->header_type = (uint8_t)(header_info >> 16);
    header_layout = device->header_type & 0x7fu;
    device->bar_count = header_layout == 0u ? 6u : (header_layout == 1u ? 2u : 0u);

    for (uint8_t bar = 0; bar < 6u; bar++) {
        device->bars[bar] = 0xffffffffu;
        if (bar < device->bar_count) {
            (void)config_read32(bus, slot, function, (uint8_t)(0x10u + bar * 4u),
                                &device->bars[bar]);
        }
    }
}

void pci_init(void)
{
    device_count = 0;

    for (uint8_t slot = 0; slot < 32u; slot++) {
        uint32_t id;
        uint32_t header_info;
        uint8_t functions = 1u;

        if (config_read32(0u, slot, 0u, 0x00u, &id) != 0 ||
            (uint16_t)id == 0xffffu || id == 0xffffffffu) {
            continue;
        }
        if (config_read32(0u, slot, 0u, 0x0cu, &header_info) == 0 &&
            ((header_info >> 16) & 0x80u) != 0) {
            functions = 8u;
        }
        for (uint8_t function = 0; function < functions; function++) {
            record_function(0u, slot, function);
        }
    }

    pci_ready = 1;
    log_info("pci: configuration mechanism #1 bus 0 scan complete");
}

const struct pci_device *pci_find_device(uint16_t vendor_id, uint16_t device_id)
{
    return pci_find_device_nth(vendor_id, device_id, 0u);
}

const struct pci_device *pci_find_device_nth(uint16_t vendor_id,
                                             uint16_t device_id,
                                             uint16_t match_index)
{
    uint16_t match = 0;

    if (!pci_ready) {
        return 0;
    }
    for (uint16_t index = 0; index < device_count; index++) {
        if (devices[index].vendor_id == vendor_id && devices[index].device_id == device_id) {
            if (match++ == match_index) {
                return &devices[index];
            }
        }
    }
    return 0;
}

int pci_get_io_bar(const struct pci_device *device, uint8_t index, uint16_t *io_base)
{
    if (!device || index >= device->bar_count) {
        return -1;
    }
    return pci_parse_io_bar(device->bars[index], io_base);
}

int pci_get_memory_bar32(const struct pci_device *device, uint8_t index,
                         uintptr_t *physical_base)
{
    if (!device || index >= device->bar_count) {
        return -1;
    }
    return pci_parse_memory_bar32(device->bars[index], physical_base);
}

static int enable_command_bits(const struct pci_device *device, uint16_t bits)
{
    uint16_t command;

    if (!device ||
        config_read16(device->bus, device->slot, device->function, 0x04u,
                      &command) != 0) {
        return -1;
    }
    command |= bits;
    if (config_write16(device->bus, device->slot, device->function, 0x04u,
                       command) != 0 ||
        config_read16(device->bus, device->slot, device->function, 0x04u,
                      &command) != 0 ||
        (command & bits) != bits) {
        return -1;
    }
    return 0;
}

int pci_enable_io_bus_master(const struct pci_device *device)
{
    return enable_command_bits(device, PCI_COMMAND_IO_SPACE |
                               PCI_COMMAND_BUS_MASTER);
}

int pci_enable_memory_bus_master(const struct pci_device *device)
{
    return enable_command_bits(device, PCI_COMMAND_MEMORY_SPACE |
                               PCI_COMMAND_BUS_MASTER);
}
