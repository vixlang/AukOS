#include "include/aukos/ethernet.h"

#include "include/aukos/e1000.h"
#include "include/aukos/log.h"
#include "include/aukos/virtio_net.h"

enum ethernet_backend {
    ETHERNET_BACKEND_NONE = 0,
    ETHERNET_BACKEND_E1000,
    ETHERNET_BACKEND_VIRTIO,
};

static enum ethernet_backend active_backend;

int ethernet_init(void)
{
    active_backend = ETHERNET_BACKEND_NONE;
    if (e1000_init() == 0) {
        active_backend = ETHERNET_BACKEND_E1000;
        log_info("ethernet: using Intel e1000 backend");
        return 0;
    }
    if (virtio_net_init() == 0) {
        active_backend = ETHERNET_BACKEND_VIRTIO;
        log_info("ethernet: using VirtIO backend");
        return 0;
    }
    log_error("ethernet: no supported PCI network device");
    return -1;
}

int ethernet_is_ready(void)
{
    if (active_backend == ETHERNET_BACKEND_E1000) {
        return e1000_is_ready();
    }
    if (active_backend == ETHERNET_BACKEND_VIRTIO) {
        return virtio_net_is_ready();
    }
    return 0;
}

int ethernet_get_mac(uint8_t mac[ETHERNET_MAC_SIZE])
{
    if (active_backend == ETHERNET_BACKEND_E1000) {
        return e1000_get_mac(mac);
    }
    if (active_backend == ETHERNET_BACKEND_VIRTIO) {
        return virtio_net_get_mac(mac);
    }
    return -1;
}

int ethernet_send_frame(const uint8_t *frame, size_t frame_length)
{
    if (active_backend == ETHERNET_BACKEND_E1000) {
        return e1000_send_frame(frame, frame_length);
    }
    if (active_backend == ETHERNET_BACKEND_VIRTIO) {
        return virtio_net_send_frame(frame, frame_length);
    }
    return -1;
}

int ethernet_poll_receive(uint8_t *frame, size_t capacity,
                          size_t *frame_length)
{
    if (active_backend == ETHERNET_BACKEND_E1000) {
        return e1000_poll_receive(frame, capacity, frame_length);
    }
    if (active_backend == ETHERNET_BACKEND_VIRTIO) {
        return virtio_net_poll_receive(frame, capacity, frame_length);
    }
    return -1;
}
