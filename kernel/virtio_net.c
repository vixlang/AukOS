#include "include/aukos/virtio_net.h"

#include "include/aukos/io.h"
#include "include/aukos/log.h"
#include "include/aukos/pci.h"
#include "include/aukos/serial.h"
#include "include/aukos/virtio_legacy.h"

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

#define VIRTIO_NET_F_MAC (1u << 5)
#define VIRTIO_NET_RECEIVE_QUEUE 0u
#define VIRTIO_NET_TRANSMIT_QUEUE 1u
#define VIRTIO_NET_POLL_LIMIT 10000000u
#define VIRTIO_NET_QUEUE_MEMORY_SIZE (3u * VIRTIO_LEGACY_QUEUE_ALIGN)
#define VIRTIO_NET_RX_BUFFER_SIZE \
    (sizeof(struct virtio_net_hdr) + VIRTIO_NET_MAX_FRAME_SIZE)

struct virtio_net_queue {
    uint16_t number;
    uint16_t size;
    uint16_t avail_index;
    uint16_t last_used_index;
    volatile struct virtio_legacy_desc *descriptors;
    volatile uint16_t *available;
    volatile uint16_t *used;
};

struct virtio_net_device {
    uint16_t io_base;
    uint8_t status;
    uint8_t mac[VIRTIO_NET_MAC_SIZE];
    int ready;
    struct virtio_net_queue receiveq;
    struct virtio_net_queue transmitq;
};

static uint8_t queue_memory[2][VIRTIO_NET_QUEUE_MEMORY_SIZE]
    __attribute__((aligned(VIRTIO_LEGACY_QUEUE_ALIGN)));
static uint8_t receive_buffer[VIRTIO_NET_RX_BUFFER_SIZE]
    __attribute__((aligned(16)));
static struct virtio_net_hdr transmit_header __attribute__((aligned(16)));
static uint8_t transmit_buffer[VIRTIO_NET_MAX_FRAME_SIZE]
    __attribute__((aligned(16)));
static struct virtio_net_device net_device;

static const uint8_t expected_mac[VIRTIO_NET_MAC_SIZE] =
    {0x52u, 0x54u, 0x00u, 0x12u, 0x34u, 0x56u};
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
        dest[index] = 0u;
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

static uint16_t reg_port(const struct virtio_net_device *device, uint16_t offset)
{
    return (uint16_t)(device->io_base + offset);
}

static int dma_address(const void *pointer, uint64_t *address)
{
    uintptr_t value = (uintptr_t)pointer;

    if (!pointer || !address || value >= 0x40000000ull) {
        return -1;
    }
    *address = value;
    return 0;
}

static void mark_failed(struct virtio_net_device *device)
{
    uint8_t status = inb(reg_port(device, VIRTIO_PCI_DEVICE_STATUS));

    status |= VIRTIO_STATUS_FAILED;
    outb(reg_port(device, VIRTIO_PCI_DEVICE_STATUS), status);
    device->status = status;
    device->ready = 0;
}

static int advance_status(struct virtio_net_device *device, uint8_t status)
{
    uint8_t observed;

    outb(reg_port(device, VIRTIO_PCI_DEVICE_STATUS), status);
    observed = inb(reg_port(device, VIRTIO_PCI_DEVICE_STATUS));
    if ((observed & status) != status ||
        (observed & VIRTIO_STATUS_FAILED) != 0u) {
        return -1;
    }
    device->status = status;
    return 0;
}

static int initialize_queue(struct virtio_net_device *device,
                            struct virtio_net_queue *queue, uint16_t number,
                            uint8_t *storage, size_t storage_size)
{
    size_t used_offset;
    size_t total_size;
    uint64_t queue_address;
    uint32_t queue_pfn;
    uint16_t offered_size;

    outw(reg_port(device, VIRTIO_PCI_QUEUE_SELECT), number);
    if (inw(reg_port(device, VIRTIO_PCI_QUEUE_SELECT)) != number) {
        serial_write("virtio-net: queue select readback mismatch requested=0x");
        serial_write_hex(number);
        serial_write(" observed=0x");
        serial_write_hex(inw(reg_port(device, VIRTIO_PCI_QUEUE_SELECT)));
        serial_write("\r\n");
        return -1;
    }
    offered_size = inw(reg_port(device, VIRTIO_PCI_QUEUE_SIZE));
    if (offered_size == 0u || offered_size > VIRTIO_LEGACY_QUEUE_MAX ||
        virtio_legacy_queue_layout(offered_size, &used_offset, &total_size) != 0 ||
        total_size > storage_size || dma_address(storage, &queue_address) != 0 ||
        (queue_address & (VIRTIO_LEGACY_QUEUE_ALIGN - 1u)) != 0u ||
        (queue_address >> 12) > UINT32_MAX) {
        return -1;
    }

    memory_zero(storage, storage_size);
    queue->number = number;
    queue->size = offered_size;
    queue->avail_index = 0u;
    queue->last_used_index = 0u;
    queue->descriptors = (volatile struct virtio_legacy_desc *)storage;
    queue->available = (volatile uint16_t *)(storage +
        (size_t)offered_size * sizeof(struct virtio_legacy_desc));
    queue->used = (volatile uint16_t *)(storage + used_offset);
    queue->available[0] = VIRTQ_AVAIL_F_NO_INTERRUPT;

    queue_pfn = (uint32_t)(queue_address >> 12);
    if (inl(reg_port(device, VIRTIO_PCI_QUEUE_PFN)) != 0u) {
        return -1;
    }
    outl(reg_port(device, VIRTIO_PCI_QUEUE_PFN), queue_pfn);
    if (inl(reg_port(device, VIRTIO_PCI_QUEUE_PFN)) != queue_pfn) {
        return -1;
    }
    return 0;
}

static int publish_receive_buffer(struct virtio_net_device *device)
{
    struct virtio_net_queue *queue = &device->receiveq;
    uint64_t buffer_address;

    if (queue->size == 0u || dma_address(receive_buffer, &buffer_address) != 0 ||
        sizeof(receive_buffer) > UINT32_MAX) {
        return -1;
    }
    queue->descriptors[0].address = buffer_address;
    queue->descriptors[0].length = (uint32_t)sizeof(receive_buffer);
    queue->descriptors[0].flags = VIRTQ_DESC_F_WRITE;
    queue->descriptors[0].next = 0u;
    queue->available[2u + (queue->avail_index % queue->size)] = 0u;
    io_memory_barrier();
    queue->avail_index++;
    queue->available[1] = queue->avail_index;
    io_memory_barrier();
    return 0;
}

static int poll_used(struct virtio_net_device *device,
                     struct virtio_net_queue *queue, uint32_t *id,
                     uint32_t *length)
{
    volatile struct virtio_legacy_used_elem *elements;
    uint16_t used_index;

    for (uint32_t spin = 0; spin < VIRTIO_NET_POLL_LIMIT; spin++) {
        used_index = queue->used[1];
        if (used_index != queue->last_used_index) {
            break;
        }
        if ((inb(reg_port(device, VIRTIO_PCI_DEVICE_STATUS)) &
             VIRTIO_STATUS_FAILED) != 0u) {
            return -1;
        }
        __asm__ volatile ("pause");
    }
    used_index = queue->used[1];
    if (used_index != (uint16_t)(queue->last_used_index + 1u)) {
        return 1;
    }

    io_memory_barrier();
    elements = (volatile struct virtio_legacy_used_elem *)&queue->used[2];
    *id = elements[queue->last_used_index % queue->size].id;
    *length = elements[queue->last_used_index % queue->size].length;
    queue->last_used_index++;
    return 0;
}

static int transport_send_frame(struct virtio_net_device *device,
                                const uint8_t *frame, size_t frame_length)
{
    struct virtio_net_queue *queue = &device->transmitq;
    uint64_t header_address;
    uint64_t frame_address;
    uint32_t used_id;
    uint32_t used_length;

    if (!device->ready || !frame || frame_length < VIRTIO_NET_MIN_FRAME_SIZE ||
        frame_length > sizeof(transmit_buffer) || queue->size < 2u ||
        (uintptr_t)frame > UINTPTR_MAX - frame_length ||
        dma_address(&transmit_header, &header_address) != 0 ||
        dma_address(transmit_buffer, &frame_address) != 0) {
        return -1;
    }

    memory_zero(&transmit_header, sizeof(transmit_header));
    memory_copy(transmit_buffer, frame, frame_length);
    queue->descriptors[0].address = header_address;
    queue->descriptors[0].length = sizeof(transmit_header);
    queue->descriptors[0].flags = VIRTQ_DESC_F_NEXT;
    queue->descriptors[0].next = 1u;
    queue->descriptors[1].address = frame_address;
    queue->descriptors[1].length = (uint32_t)frame_length;
    queue->descriptors[1].flags = 0u;
    queue->descriptors[1].next = 0u;

    queue->available[2u + (queue->avail_index % queue->size)] = 0u;
    io_memory_barrier();
    queue->avail_index++;
    queue->available[1] = queue->avail_index;
    io_memory_barrier();
    outw(reg_port(device, VIRTIO_PCI_QUEUE_NOTIFY), queue->number);

    if (poll_used(device, queue, &used_id, &used_length) != 0) {
        serial_write("virtio-net: TX completion timeout/status=0x");
        serial_write_hex(inb(reg_port(device, VIRTIO_PCI_DEVICE_STATUS)));
        serial_write(" used=0x");
        serial_write_hex(queue->used[1]);
        serial_write("\r\n");
        mark_failed(device);
        return -1;
    }
    if (used_id != 0u || used_id >= queue->size || used_length != 0u) {
        serial_write("virtio-net: invalid TX completion id=0x");
        serial_write_hex(used_id);
        serial_write(" length=0x");
        serial_write_hex(used_length);
        serial_write("\r\n");
        mark_failed(device);
        return -1;
    }
    return 0;
}

static int transport_poll_receive(struct virtio_net_device *device,
                                  uint8_t *frame, size_t capacity,
                                  size_t *frame_length)
{
    struct virtio_net_queue *queue = &device->receiveq;
    const struct virtio_net_hdr *header;
    uint32_t used_id;
    uint32_t used_length;
    size_t payload_length;

    int completion_invalid = 0;
    int result = -1;

    if (!device->ready || !frame || !frame_length || queue->size == 0u ||
        (uintptr_t)frame > UINTPTR_MAX - capacity) {
        return -1;
    }
    result = poll_used(device, queue, &used_id, &used_length);
    if (result > 0) {
        return 1;
    }
    if (result < 0) {
        mark_failed(device);
        return -1;
    }
    if (used_id != 0u || used_id >= queue->size ||
        used_length < sizeof(struct virtio_net_hdr) +
                      VIRTIO_NET_ETHERNET_HEADER_SIZE ||
        used_length > sizeof(receive_buffer)) {
        completion_invalid = 1;
    } else {
        header = (const struct virtio_net_hdr *)receive_buffer;
        payload_length = used_length - sizeof(*header);
        if (header->flags != 0u || header->gso_type != 0u ||
            header->header_length != 0u || header->gso_size != 0u ||
            header->checksum_start != 0u || header->checksum_offset != 0u) {
            completion_invalid = 1;
        } else if (payload_length <= capacity) {
            memory_copy(frame, receive_buffer + sizeof(*header),
                        payload_length);
            *frame_length = payload_length;
            result = 0;
        }
    }

    if (publish_receive_buffer(device) != 0) {
        mark_failed(device);
        return -1;
    }
    outw(reg_port(device, VIRTIO_PCI_QUEUE_NOTIFY), queue->number);
    if (completion_invalid) {
        mark_failed(device);
    }
    return result;
}

int virtio_net_init(void)
{
    const struct pci_device *pci_device;
    uint32_t host_features;
    int reset_complete = 0;

    memory_zero(&net_device, sizeof(net_device));
    pci_device = pci_find_device(PCI_VENDOR_VIRTIO,
                                 PCI_DEVICE_VIRTIO_NET_TRANSITIONAL);
    if (!pci_device || pci_get_io_bar(pci_device, 0u, &net_device.io_base) != 0 ||
        net_device.io_base > UINT16_MAX - (VIRTIO_PCI_CONFIG + 5u) ||
        pci_enable_io_bus_master(pci_device) != 0) {
        log_error("virtio-net: transitional PCI device unavailable");
        return -1;
    }

    serial_write("pci: found virtio-net 1af4:1000 io=0x");
    serial_write_hex(net_device.io_base);
    serial_write("\r\n");

    outb(reg_port(&net_device, VIRTIO_PCI_DEVICE_STATUS), 0u);
    for (uint32_t spin = 0; spin < VIRTIO_NET_POLL_LIMIT; spin++) {
        if (inb(reg_port(&net_device, VIRTIO_PCI_DEVICE_STATUS)) == 0u) {
            reset_complete = 1;
            break;
        }
        __asm__ volatile ("pause");
    }
    if (!reset_complete) {
        mark_failed(&net_device);
        log_error("virtio-net: device reset timed out");
        return -1;
    }

    if (advance_status(&net_device, VIRTIO_STATUS_ACKNOWLEDGE) != 0 ||
        advance_status(&net_device, VIRTIO_STATUS_ACKNOWLEDGE |
                                    VIRTIO_STATUS_DRIVER) != 0) {
        mark_failed(&net_device);
        log_error("virtio-net: device status handshake failed");
        return -1;
    }

    host_features = inl(reg_port(&net_device, VIRTIO_PCI_HOST_FEATURES));
    if ((host_features & VIRTIO_NET_F_MAC) == 0u) {
        mark_failed(&net_device);
        log_error("virtio-net: device does not provide a MAC address");
        return -1;
    }
    outl(reg_port(&net_device, VIRTIO_PCI_GUEST_FEATURES), VIRTIO_NET_F_MAC);
    for (size_t index = 0; index < VIRTIO_NET_MAC_SIZE; index++) {
        net_device.mac[index] = inb(reg_port(&net_device,
                                            VIRTIO_PCI_CONFIG + index));
    }
    if (!memory_equal(net_device.mac, expected_mac, sizeof(expected_mac))) {
        mark_failed(&net_device);
        log_error("virtio-net: configured MAC does not match QEMU test MAC");
        return -1;
    }
    log_info("virtio-net: MAC 52:54:00:12:34:56");

    if (initialize_queue(&net_device, &net_device.receiveq,
                         VIRTIO_NET_RECEIVE_QUEUE, queue_memory[0],
                         sizeof(queue_memory[0])) != 0 ||
        initialize_queue(&net_device, &net_device.transmitq,
                         VIRTIO_NET_TRANSMIT_QUEUE, queue_memory[1],
                         sizeof(queue_memory[1])) != 0 ||
        net_device.transmitq.size < 2u ||
        publish_receive_buffer(&net_device) != 0) {
        mark_failed(&net_device);
        log_error("virtio-net: invalid receiveq or transmitq");
        return -1;
    }

    if (advance_status(&net_device, VIRTIO_STATUS_ACKNOWLEDGE |
                                    VIRTIO_STATUS_DRIVER |
                                    VIRTIO_STATUS_DRIVER_OK) != 0) {
        mark_failed(&net_device);
        log_error("virtio-net: device rejected DRIVER_OK");
        return -1;
    }
    net_device.ready = 1;
    outw(reg_port(&net_device, VIRTIO_PCI_QUEUE_NOTIFY),
         net_device.receiveq.number);
    log_info("virtio-net: receiveq/transmitq initialized (legacy split, polling)");

    return 0;
}

int virtio_net_is_ready(void)
{
    return net_device.ready;
}

int virtio_net_get_mac(uint8_t mac[VIRTIO_NET_MAC_SIZE])
{
    if (!net_device.ready || !mac) {
        return -1;
    }
    memory_copy(mac, net_device.mac, VIRTIO_NET_MAC_SIZE);
    return 0;
}

int virtio_net_send_frame(const uint8_t *frame, size_t frame_length)
{
    return transport_send_frame(&net_device, frame, frame_length);
}

int virtio_net_poll_receive(uint8_t *frame, size_t capacity,
                            size_t *frame_length)
{
    return transport_poll_receive(&net_device, frame, capacity, frame_length);
}
