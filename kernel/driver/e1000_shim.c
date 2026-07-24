#include "../include/aukos/e1000.h"

#include "../include/aukos/io.h"
#include "../include/aukos/pci.h"
#include "../include/aukos/serial.h"
#include "../include/aukos/timer.h"
#include "../include/aukos/vmm.h"

#include <stddef.h>
#include <stdint.h>

#define E1000_VENDOR_INTEL 0x8086u
#define E1000_IOADDR 0x00u
#define E1000_IODATA 0x04u
#define E1000_MMIO_SIZE 0x20000u
#define E1000_MMIO_VIRTUAL_BASE 0xffff900000000000ull

enum e1000_state_slot {
    E1000_STATE_READY = 0,
    E1000_STATE_RX_INDEX = 1,
    E1000_STATE_TX_INDEX = 2,
};

static struct e1000_rx_descriptor
    rx_descriptors[E1000_RX_DESCRIPTOR_COUNT] __attribute__((aligned(16)));
static struct e1000_tx_descriptor
    tx_descriptors[E1000_TX_DESCRIPTOR_COUNT] __attribute__((aligned(16)));
static uint8_t rx_buffers[E1000_RX_DESCRIPTOR_COUNT][E1000_RX_BUFFER_SIZE]
    __attribute__((aligned(16)));
static uint8_t tx_buffers[E1000_TX_DESCRIPTOR_COUNT][E1000_MAX_FRAME_SIZE]
    __attribute__((aligned(16)));
static uint8_t device_mac[E1000_MAC_SIZE];
static uint32_t driver_state[3];
static uint16_t io_base;
static volatile uint8_t *mmio_base;

static void bytes_zero(void *destination, size_t size)
{
    uint8_t *bytes = destination;

    for (size_t index = 0; index < size; index++) {
        bytes[index] = 0u;
    }
}

static void bytes_copy(void *destination, const void *source, size_t size)
{
    uint8_t *dest = destination;
    const uint8_t *src = source;

    for (size_t index = 0; index < size; index++) {
        dest[index] = src[index];
    }
}

static const struct pci_device *find_device(void)
{
    static const uint16_t supported_ids[] = {
        0x100eu, /* 82540EM, QEMU e1000 */
        0x100fu, /* 82545EM */
        0x1010u, /* 82546EB */
        0x107cu, /* 82541PI */
        0x10d3u, /* 82574L */
    };

    for (size_t index = 0; index < sizeof(supported_ids) / sizeof(supported_ids[0]);
         index++) {
        const struct pci_device *device =
            pci_find_device(E1000_VENDOR_INTEL, supported_ids[index]);
        if (device) {
            return device;
        }
    }
    return 0;
}

int e1000_shim_prepare(void)
{
    const struct pci_device *device = find_device();
    uintptr_t mmio_physical;

    io_base = 0u;
    mmio_base = 0;
    bytes_zero(driver_state, sizeof(driver_state));
    bytes_zero(device_mac, sizeof(device_mac));
    if (!device) {
        return -1;
    }
    serial_write("pci: found e1000 8086:");
    serial_write_hex(device->device_id);
    serial_write("\r\n");
    if (pci_get_memory_bar32(device, 0u, &mmio_physical) == 0 &&
        pci_enable_memory_bus_master(device) == 0) {
        struct address_space *kernel_space =
            (struct address_space *)(uintptr_t)vmm_kernel_address_space();

        for (uintptr_t offset = 0; offset < E1000_MMIO_SIZE; offset += 4096u) {
            if (vmm_map_page(kernel_space, E1000_MMIO_VIRTUAL_BASE + offset,
                             mmio_physical + offset,
                             VMM_USER_FLAG_READABLE |
                                 VMM_USER_FLAG_WRITABLE) != 0) {
                return -2;
            }
        }
        mmio_base = (volatile uint8_t *)(uintptr_t)E1000_MMIO_VIRTUAL_BASE;
        serial_write("e1000: MMIO base=0x");
        serial_write_hex(mmio_physical);
        serial_write("\r\n");
        return 0;
    }
    if (pci_enable_io_bus_master(device) != 0) {
        return -2;
    }
    for (uint8_t bar = 0u; bar < device->bar_count; bar++) {
        if (pci_get_io_bar(device, bar, &io_base) == 0 &&
            io_base <= UINT16_MAX - E1000_IODATA) {
            serial_write("e1000: register I/O base=0x");
            serial_write_hex(io_base);
            serial_write("\r\n");
            return 0;
        }
    }
    io_base = 0u;
    return -3;
}

uint32_t e1000_shim_reg_read(uint32_t offset)
{
    if ((offset & 3u) != 0u || offset >= E1000_MMIO_SIZE) {
        return UINT32_MAX;
    }
    if (mmio_base) {
        return *(volatile uint32_t *)(mmio_base + offset);
    }
    if (io_base == 0u) {
        return UINT32_MAX;
    }
    outl((uint16_t)(io_base + E1000_IOADDR), offset);
    return inl((uint16_t)(io_base + E1000_IODATA));
}

void e1000_shim_reg_write(uint32_t offset, uint32_t value)
{
    if ((offset & 3u) != 0u || offset >= E1000_MMIO_SIZE) {
        return;
    }
    if (mmio_base) {
        *(volatile uint32_t *)(mmio_base + offset) = value;
        return;
    }
    if (io_base == 0u) {
        return;
    }
    outl((uint16_t)(io_base + E1000_IOADDR), offset);
    outl((uint16_t)(io_base + E1000_IODATA), value);
}

void e1000_shim_pause(void)
{
    __asm__ volatile ("pause");
}

void e1000_shim_wait_timer_ticks(uint32_t count)
{
    uint64_t flags;
    uint64_t start = timer_ticks();

    __asm__ volatile ("pushfq; popq %0" : "=r"(flags));
    __asm__ volatile ("sti" : : : "memory");
    while (timer_ticks() - start < count) {
        __asm__ volatile ("hlt");
    }
    if ((flags & (1ull << 9)) == 0u) {
        __asm__ volatile ("cli" : : : "memory");
    }
}

void e1000_shim_barrier(void)
{
    io_memory_barrier();
}

uint32_t e1000_shim_ring_address_low(int transmit)
{
    uintptr_t address = transmit ? (uintptr_t)tx_descriptors
                                 : (uintptr_t)rx_descriptors;
    return (uint32_t)address;
}

uint32_t e1000_shim_ring_address_high(int transmit)
{
    uintptr_t address = transmit ? (uintptr_t)tx_descriptors
                                 : (uintptr_t)rx_descriptors;
    return (uint32_t)(address >> 32);
}

int e1000_shim_reset_rings(void)
{
    bytes_zero(rx_descriptors, sizeof(rx_descriptors));
    bytes_zero(tx_descriptors, sizeof(tx_descriptors));
    bytes_zero(rx_buffers, sizeof(rx_buffers));
    bytes_zero(tx_buffers, sizeof(tx_buffers));

    for (size_t index = 0; index < E1000_RX_DESCRIPTOR_COUNT; index++) {
        rx_descriptors[index].address = (uintptr_t)rx_buffers[index];
    }
    for (size_t index = 0; index < E1000_TX_DESCRIPTOR_COUNT; index++) {
        tx_descriptors[index].address = (uintptr_t)tx_buffers[index];
        tx_descriptors[index].status = 1u;
    }
    driver_state[E1000_STATE_RX_INDEX] = 0u;
    driver_state[E1000_STATE_TX_INDEX] = 0u;
    return 0;
}

uint32_t e1000_shim_state_get(uint32_t slot)
{
    if (slot >= sizeof(driver_state) / sizeof(driver_state[0])) {
        return 0u;
    }
    return driver_state[slot];
}

void e1000_shim_state_set(uint32_t slot, uint32_t value)
{
    if (slot < sizeof(driver_state) / sizeof(driver_state[0])) {
        driver_state[slot] = value;
    }
}

void e1000_shim_mac_set(uint32_t index, uint32_t value)
{
    if (index < E1000_MAC_SIZE) {
        device_mac[index] = (uint8_t)value;
    }
}

int e1000_shim_mac_copy(uint8_t *destination)
{
    if (!destination || driver_state[E1000_STATE_READY] == 0u) {
        return -1;
    }
    bytes_copy(destination, device_mac, sizeof(device_mac));
    return 0;
}

uint32_t e1000_shim_rx_status(uint32_t index)
{
    volatile struct e1000_rx_descriptor *descriptors = rx_descriptors;
    return index < E1000_RX_DESCRIPTOR_COUNT ? descriptors[index].status : 0u;
}

uint32_t e1000_shim_rx_errors(uint32_t index)
{
    volatile struct e1000_rx_descriptor *descriptors = rx_descriptors;
    return index < E1000_RX_DESCRIPTOR_COUNT ? descriptors[index].errors : 0xffu;
}

uint32_t e1000_shim_rx_length(uint32_t index)
{
    volatile struct e1000_rx_descriptor *descriptors = rx_descriptors;
    return index < E1000_RX_DESCRIPTOR_COUNT ? descriptors[index].length : 0u;
}

int e1000_shim_rx_copy(uint32_t index, uint8_t *destination, uint32_t length)
{
    if (index >= E1000_RX_DESCRIPTOR_COUNT || !destination ||
        length > E1000_RX_BUFFER_SIZE) {
        return -1;
    }
    bytes_copy(destination, rx_buffers[index], length);
    return 0;
}

void e1000_shim_store_size(size_t *destination, size_t value)
{
    if (destination) {
        *destination = value;
    }
}

void e1000_shim_rx_rearm(uint32_t index)
{
    volatile struct e1000_rx_descriptor *descriptors = rx_descriptors;

    if (index >= E1000_RX_DESCRIPTOR_COUNT) {
        return;
    }
    descriptors[index].length = 0u;
    descriptors[index].checksum = 0u;
    descriptors[index].errors = 0u;
    descriptors[index].special = 0u;
    io_memory_barrier();
    descriptors[index].status = 0u;
}

uint32_t e1000_shim_tx_status(uint32_t index)
{
    volatile struct e1000_tx_descriptor *descriptors = tx_descriptors;
    return index < E1000_TX_DESCRIPTOR_COUNT ? descriptors[index].status : 0u;
}

int e1000_shim_tx_submit(uint32_t index, const uint8_t *source, size_t length)
{
    volatile struct e1000_tx_descriptor *descriptor;

    if (index >= E1000_TX_DESCRIPTOR_COUNT || !source ||
        length < E1000_MIN_FRAME_SIZE || length > E1000_MAX_FRAME_SIZE) {
        return -1;
    }
    descriptor = &tx_descriptors[index];
    bytes_copy(tx_buffers[index], source, length);
    descriptor->length = (uint16_t)length;
    descriptor->checksum_offset = 0u;
    descriptor->checksum_start = 0u;
    descriptor->special = 0u;
    descriptor->command = 0x0bu; /* EOP | IFCS | RS */
    io_memory_barrier();
    descriptor->status = 0u;
    return 0;
}
