#include "include/aukos/virtio_blk.h"

#include "include/aukos/block.h"
#include "include/aukos/io.h"
#include "include/aukos/log.h"
#include "include/aukos/pci.h"
#include "include/aukos/serial.h"

#include <stddef.h>
#include <stdint.h>

#define VIRTIO_PCI_HOST_FEATURES 0x00u
#define VIRTIO_PCI_GUEST_FEATURES 0x04u
#define VIRTIO_PCI_QUEUE_PFN 0x08u
#define VIRTIO_PCI_QUEUE_SIZE 0x0cu
#define VIRTIO_PCI_QUEUE_SELECT 0x0eu
#define VIRTIO_PCI_QUEUE_NOTIFY 0x10u
#define VIRTIO_PCI_DEVICE_STATUS 0x12u
#define VIRTIO_PCI_CONFIG 0x14u

#define VIRTIO_STATUS_ACKNOWLEDGE 0x01u
#define VIRTIO_STATUS_DRIVER 0x02u
#define VIRTIO_STATUS_DRIVER_OK 0x04u
#define VIRTIO_STATUS_FAILED 0x80u

#define VIRTIO_BLK_F_RO (1u << 5)
#define VIRTIO_BLK_F_FLUSH (1u << 9)
#define VIRTIO_BLK_T_IN 0u
#define VIRTIO_BLK_T_OUT 1u
#define VIRTIO_BLK_T_FLUSH 4u
#define VIRTIO_BLK_S_OK 0u
#define VIRTIO_POLL_LIMIT 500000000u
#define VIRTIO_QUEUE_MEMORY_SIZE (3u * VIRTIO_LEGACY_QUEUE_ALIGN)
#define VIRTIO_BLK_MAX_DEVICES 2u
#define VIRTIO_BLK_MAX_REQUEST_SECTORS 8u
#define VIRTIO_BLK_BOUNCE_SIZE \
    (VIRTIO_BLK_MAX_REQUEST_SECTORS * BLOCK_SECTOR_SIZE)

struct virtio_blk_request_header {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

_Static_assert(sizeof(struct virtio_blk_request_header) == 16u, "virtio block header size");

struct virtio_blk_device {
    uint16_t io_base;
    uint16_t queue_size;
    uint16_t avail_index;
    uint16_t last_used_index;
    uint64_t capacity;
    int ready;
    int busy;
    int read_only;
    int has_flush;
    volatile struct virtio_legacy_desc *descriptors;
    volatile uint16_t *available;
    volatile uint16_t *used;
    uint8_t *queue_memory;
    struct virtio_blk_request_header *request_header;
    uint8_t *bounce_buffer;
    volatile uint8_t *request_status;
    struct block_device block;
};

static uint8_t queue_memory[VIRTIO_BLK_MAX_DEVICES][VIRTIO_QUEUE_MEMORY_SIZE]
    __attribute__((aligned(VIRTIO_LEGACY_QUEUE_ALIGN)));
static struct virtio_blk_request_header request_headers[VIRTIO_BLK_MAX_DEVICES]
    __attribute__((aligned(16)));
static uint8_t bounce_buffers[VIRTIO_BLK_MAX_DEVICES][VIRTIO_BLK_BOUNCE_SIZE]
    __attribute__((aligned(16)));
static volatile uint8_t request_statuses[VIRTIO_BLK_MAX_DEVICES]
    __attribute__((aligned(16)));
static struct virtio_blk_device virtio_devices[VIRTIO_BLK_MAX_DEVICES];

static void memory_copy(void *destination, const void *source, size_t size)
{
    uint8_t *dest = (uint8_t *)destination;
    const uint8_t *src = (const uint8_t *)source;

    for (size_t index = 0; index < size; index++) {
        dest[index] = src[index];
    }
}

static void memory_zero(void *destination, size_t size)
{
    uint8_t *dest = (uint8_t *)destination;

    for (size_t index = 0; index < size; index++) {
        dest[index] = 0;
    }
}

static int memory_equal(const uint8_t *left, const uint8_t *right, size_t size)
{
    for (size_t index = 0; index < size; index++) {
        if (left[index] != right[index]) {
            return 0;
        }
    }
    return 1;
}

static uint16_t reg_port(const struct virtio_blk_device *device, uint16_t offset)
{
    return (uint16_t)(device->io_base + offset);
}

static void mark_failed(struct virtio_blk_device *device)
{
    uint8_t status = inb(reg_port(device, VIRTIO_PCI_DEVICE_STATUS));

    outb(reg_port(device, VIRTIO_PCI_DEVICE_STATUS), status | VIRTIO_STATUS_FAILED);
    device->ready = 0;
}

static int advance_status(struct virtio_blk_device *device, uint8_t status)
{
    uint8_t observed;

    outb(reg_port(device, VIRTIO_PCI_DEVICE_STATUS), status);
    observed = inb(reg_port(device, VIRTIO_PCI_DEVICE_STATUS));
    return (observed & status) == status &&
           (observed & VIRTIO_STATUS_FAILED) == 0 ? 0 : -1;
}

static int dma_address(const void *pointer, uint64_t *address)
{
    uintptr_t value = (uintptr_t)pointer;

    if (!address || value >= 0x40000000ull) {
        return -1;
    }
    *address = value;
    return 0;
}

static int initialize_queue(struct virtio_blk_device *device)
{
    size_t used_offset;
    size_t total_size;
    uint64_t queue_address;
    uint32_t queue_pfn;
    uint16_t offered_size;

    outw(reg_port(device, VIRTIO_PCI_QUEUE_SELECT), 0u);
    offered_size = inw(reg_port(device, VIRTIO_PCI_QUEUE_SIZE));
    if (offered_size == 0u || offered_size > VIRTIO_LEGACY_QUEUE_MAX ||
        virtio_legacy_queue_layout(offered_size, &used_offset, &total_size) != 0 ||
        total_size > VIRTIO_QUEUE_MEMORY_SIZE ||
        dma_address(device->queue_memory, &queue_address) != 0 ||
        (queue_address & (VIRTIO_LEGACY_QUEUE_ALIGN - 1u)) != 0 ||
        (queue_address >> 12) > UINT32_MAX) {
        return -1;
    }

    memory_zero(device->queue_memory, VIRTIO_QUEUE_MEMORY_SIZE);
    device->queue_size = offered_size;
    device->avail_index = 0;
    device->last_used_index = 0;
    device->descriptors = (volatile struct virtio_legacy_desc *)device->queue_memory;
    device->available = (volatile uint16_t *)(device->queue_memory +
        (size_t)offered_size * sizeof(struct virtio_legacy_desc));
    device->used = (volatile uint16_t *)(device->queue_memory + used_offset);

    queue_pfn = (uint32_t)(queue_address >> 12);
    outl(reg_port(device, VIRTIO_PCI_QUEUE_PFN), 0u);
    outl(reg_port(device, VIRTIO_PCI_QUEUE_PFN), queue_pfn);
    if (inl(reg_port(device, VIRTIO_PCI_QUEUE_PFN)) != queue_pfn) {
        return -1;
    }
    return 0;
}

static int submit_request(struct virtio_blk_device *device, uint32_t type,
                          uint64_t sector, void *buffer, size_t sector_count)
{
    volatile struct virtio_legacy_used_elem *used_elements;
    uint16_t used_index;
    uint32_t expected_length;
    uint64_t header_address;
    uint64_t data_address;
    uint64_t status_address;

    int is_flush = type == VIRTIO_BLK_T_FLUSH;
    size_t byte_count = sector_count * BLOCK_SECTOR_SIZE;

    if (!device->ready || device->busy ||
        (!is_flush && (!buffer || !sector_count ||
                       sector_count > VIRTIO_BLK_MAX_REQUEST_SECTORS ||
                       sector > device->capacity ||
                       sector_count > device->capacity - sector)) ||
        (is_flush && sector_count != 0u) ||
        (type == VIRTIO_BLK_T_OUT && device->read_only) ||
        (is_flush && !device->has_flush) ||
        (type != VIRTIO_BLK_T_IN && type != VIRTIO_BLK_T_OUT && !is_flush) ||
        dma_address(device->request_header, &header_address) != 0 ||
        (!is_flush && dma_address(device->bounce_buffer, &data_address) != 0) ||
        dma_address((const void *)device->request_status, &status_address) != 0) {
        return -1;
    }
    device->busy = 1;

    device->request_header->type = type;
    device->request_header->reserved = 0;
    device->request_header->sector = is_flush ? 0u : sector;
    *device->request_status = 0xffu;
    if (type == VIRTIO_BLK_T_OUT) {
        memory_copy(device->bounce_buffer, buffer, byte_count);
    }

    device->descriptors[0].address = header_address;
    device->descriptors[0].length = sizeof(*device->request_header);
    device->descriptors[0].flags = VIRTQ_DESC_F_NEXT;
    device->descriptors[0].next = is_flush ? 1u : 1u;
    if (is_flush) {
        device->descriptors[1].address = status_address;
        device->descriptors[1].length = 1u;
        device->descriptors[1].flags = VIRTQ_DESC_F_WRITE;
        device->descriptors[1].next = 0u;
    } else {
        device->descriptors[1].address = data_address;
        device->descriptors[1].length = (uint32_t)byte_count;
        device->descriptors[1].flags = VIRTQ_DESC_F_NEXT |
            (type == VIRTIO_BLK_T_IN ? VIRTQ_DESC_F_WRITE : 0u);
        device->descriptors[1].next = 2u;
        device->descriptors[2].address = status_address;
        device->descriptors[2].length = 1u;
        device->descriptors[2].flags = VIRTQ_DESC_F_WRITE;
        device->descriptors[2].next = 0u;
    }

    device->available[2u + (device->avail_index % device->queue_size)] = 0u;
    io_memory_barrier();
    device->avail_index++;
    device->available[1] = device->avail_index;
    io_memory_barrier();
    outw(reg_port(device, VIRTIO_PCI_QUEUE_NOTIFY), 0u);

    for (uint32_t spin = 0; spin < VIRTIO_POLL_LIMIT; spin++) {
        if (device->used[1] != device->last_used_index) {
            break;
        }
        if ((spin & 0x3ffu) == 0u &&
            (inb(reg_port(device, VIRTIO_PCI_DEVICE_STATUS)) &
             VIRTIO_STATUS_FAILED) != 0u) {
            break;
        }
        __asm__ volatile ("pause");
    }
    used_index = device->used[1];
    if (used_index == device->last_used_index) {
        device->busy = 0;
        mark_failed(device);
        log_error("virtio-blk: request polling timeout");
        return -1;
    }

    io_memory_barrier();
    used_elements = (volatile struct virtio_legacy_used_elem *)&device->used[2];
    expected_length = type == VIRTIO_BLK_T_IN ? (uint32_t)byte_count + 1u : 1u;
    if (used_index != (uint16_t)(device->last_used_index + 1u) ||
        used_elements[device->last_used_index % device->queue_size].id != 0u ||
        used_elements[device->last_used_index % device->queue_size].length != expected_length ||
        *device->request_status != VIRTIO_BLK_S_OK) {
        device->busy = 0;
        mark_failed(device);
        log_error("virtio-blk: request completion validation failed");
        return -1;
    }
    device->last_used_index++;

    if (type == VIRTIO_BLK_T_IN) {
        memory_copy(buffer, device->bounce_buffer, byte_count);
    }
    device->busy = 0;
    return 0;
}

static int transfer(struct block_device *block, uint64_t sector, void *buffer,
                    size_t sector_count, uint32_t type)
{
    struct virtio_blk_device *device;
    uint8_t *bytes = (uint8_t *)buffer;
    size_t byte_count;

    if (!block || (!buffer && sector_count != 0) ||
        sector_count > SIZE_MAX / BLOCK_SECTOR_SIZE ||
        sector > block->sector_count || sector_count > block->sector_count - sector) {
        return -1;
    }
    byte_count = sector_count * BLOCK_SECTOR_SIZE;
    if ((uintptr_t)buffer > UINTPTR_MAX - byte_count) {
        return -1;
    }
    device = (struct virtio_blk_device *)block->private_data;
    if (!device || !device->ready) {
        return -1;
    }

    for (size_t index = 0; index < sector_count;) {
        size_t chunk = sector_count - index;
        if (chunk > VIRTIO_BLK_MAX_REQUEST_SECTORS)
            chunk = VIRTIO_BLK_MAX_REQUEST_SECTORS;
        if (submit_request(device, type, sector + index,
                           bytes + index * BLOCK_SECTOR_SIZE, chunk) != 0) {
            return -1;
        }
        index += chunk;
    }
    return 0;
}

static int virtio_read(struct block_device *block, uint64_t sector, void *buffer,
                       size_t sector_count)
{
    return transfer(block, sector, buffer, sector_count, VIRTIO_BLK_T_IN);
}

static int virtio_write(struct block_device *block, uint64_t sector,
                        const void *buffer, size_t sector_count)
{
    return transfer(block, sector, (void *)buffer, sector_count, VIRTIO_BLK_T_OUT);
}

static int virtio_flush(struct block_device *block)
{
    struct virtio_blk_device *device;

    if (!block || !(device = (struct virtio_blk_device *)block->private_data)) {
        return -1;
    }
    return submit_request(device, VIRTIO_BLK_T_FLUSH, 0u, 0, 0u);
}

static int device_selftest(struct virtio_blk_device *device)
{
    uint8_t known_sector[BLOCK_SECTOR_SIZE];
    uint8_t original[BLOCK_SECTOR_SIZE];
    uint8_t pattern[BLOCK_SECTOR_SIZE];
    uint8_t readback[BLOCK_SECTOR_SIZE];
    uint64_t scratch_sector = device->capacity - 1u;

    if (device->capacity != VIRTIO_BLK_FIXTURE_SECTORS ||
        submit_request(device, VIRTIO_BLK_T_IN, 2u, known_sector, 1u) != 0 ||
        known_sector[56] != 0x53u || known_sector[57] != 0xefu ||
        submit_request(device, VIRTIO_BLK_T_IN, scratch_sector, original, 1u) != 0) {
        return -1;
    }
    for (size_t index = 0; index < BLOCK_SECTOR_SIZE; index++) {
        pattern[index] = (uint8_t)(index ^ 0x5au);
        readback[index] = 0;
    }
    if (submit_request(device, VIRTIO_BLK_T_OUT, scratch_sector, pattern, 1u) != 0 ||
        submit_request(device, VIRTIO_BLK_T_IN, scratch_sector, readback, 1u) != 0 ||
        !memory_equal(pattern, readback, sizeof(pattern)) ||
        submit_request(device, VIRTIO_BLK_T_OUT, scratch_sector, original, 1u) != 0) {
        return -1;
    }
    return 0;
}

static int initialize_device(uint16_t index, const struct pci_device *pci_device)
{
    static const char *const names[VIRTIO_BLK_MAX_DEVICES] = {
        "virtio0", "virtio1"
    };
    struct virtio_blk_device *device = &virtio_devices[index];
    uint32_t host_features;
    uint32_t capacity_low;
    uint32_t capacity_high;
    uint8_t status;

    memory_zero(device, sizeof(*device));
    device->queue_memory = queue_memory[index];
    device->request_header = &request_headers[index];
    device->bounce_buffer = bounce_buffers[index];
    device->request_status = &request_statuses[index];
    if (!pci_device || pci_get_io_bar(pci_device, 0u, &device->io_base) != 0 ||
        device->io_base > UINT16_MAX - (VIRTIO_PCI_CONFIG + 7u) ||
        pci_enable_io_bus_master(pci_device) != 0) {
        log_error("virtio-blk: transitional PCI device unavailable");
        return -1;
    }

    serial_write("pci: found virtio-blk 1af4:1001 io=0x");
    serial_write_hex(device->io_base);
    serial_write("\r\n");

    outb(reg_port(device, VIRTIO_PCI_DEVICE_STATUS), 0u);
    for (uint32_t spin = 0; spin < VIRTIO_POLL_LIMIT; spin++) {
        if (inb(reg_port(device, VIRTIO_PCI_DEVICE_STATUS)) == 0u) {
            break;
        }
        if (spin + 1u == VIRTIO_POLL_LIMIT) {
            log_error("virtio-blk: device reset timed out");
            return -1;
        }
    }

    status = VIRTIO_STATUS_ACKNOWLEDGE;
    if (advance_status(device, status) != 0) {
        mark_failed(device);
        log_error("virtio-blk: ACKNOWLEDGE status rejected");
        return -1;
    }
    status |= VIRTIO_STATUS_DRIVER;
    if (advance_status(device, status) != 0) {
        mark_failed(device);
        log_error("virtio-blk: DRIVER status rejected");
        return -1;
    }

    host_features = inl(reg_port(device, VIRTIO_PCI_HOST_FEATURES));
    device->read_only = (host_features & VIRTIO_BLK_F_RO) != 0;
    device->has_flush = (host_features & VIRTIO_BLK_F_FLUSH) != 0;
    outl(reg_port(device, VIRTIO_PCI_GUEST_FEATURES),
         device->has_flush ? VIRTIO_BLK_F_FLUSH : 0u);

    capacity_low = inl(reg_port(device, VIRTIO_PCI_CONFIG));
    capacity_high = inl(reg_port(device, VIRTIO_PCI_CONFIG + 4u));
    device->capacity = (uint64_t)capacity_low | ((uint64_t)capacity_high << 32);
    if (device->capacity == 0u ||
        device->capacity > UINT64_MAX / BLOCK_SECTOR_SIZE ||
        initialize_queue(device) != 0) {
        mark_failed(device);
        log_error("virtio-blk: invalid capacity or legacy queue");
        return -1;
    }

    status |= VIRTIO_STATUS_DRIVER_OK;
    if (advance_status(device, status) != 0) {
        mark_failed(device);
        log_error("virtio-blk: device rejected DRIVER_OK");
        return -1;
    }
    device->ready = 1;

    if (index == 0u && device_selftest(device) != 0) {
        mark_failed(device);
        log_error("virtio-blk: read/write self-test failed");
        return -1;
    }

    device->block.name = names[index];
    device->block.sector_count = device->capacity;
    device->block.sector_size = BLOCK_SECTOR_SIZE;
    device->block.private_data = device;
    device->block.read = virtio_read;
    device->block.write = virtio_write;
    device->block.flush = device->has_flush ? virtio_flush : 0;
    if (block_register(&device->block) != 0) {
        mark_failed(device);
        log_error("virtio-blk: block device registration failed");
        return -1;
    }
    return 0;
}

int virtio_blk_init(void)
{
    uint16_t initialized = 0;

    for (uint16_t index = 0; index < VIRTIO_BLK_MAX_DEVICES; index++) {
        const struct pci_device *pci_device = pci_find_device_nth(
            PCI_VENDOR_VIRTIO, PCI_DEVICE_VIRTIO_BLK_TRANSITIONAL, index);

        if (!pci_device) {
            break;
        }
        if (initialize_device(index, pci_device) != 0) {
            log_error("virtio-blk: device initialization failed");
            continue;
        }
        initialized++;
        if (index == 0u) {
            log_info("virtio-blk: virtio0 fixture self-test passed");
        } else {
            log_info("virtio-blk: virtio1 work device initialized with FLUSH");
        }
    }
    return initialized != 0u ? 0 : -1;
}
