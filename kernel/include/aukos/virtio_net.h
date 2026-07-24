#ifndef AUKOS_VIRTIO_NET_H
#define AUKOS_VIRTIO_NET_H

#include <stddef.h>
#include <stdint.h>

#define VIRTIO_NET_MAC_SIZE 6u
#define VIRTIO_NET_ETHERNET_HEADER_SIZE 14u
#define VIRTIO_NET_MIN_FRAME_SIZE 60u
#define VIRTIO_NET_MAX_FRAME_SIZE 1514u

struct virtio_net_hdr {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t header_length;
    uint16_t gso_size;
    uint16_t checksum_start;
    uint16_t checksum_offset;
} __attribute__((packed));

_Static_assert(sizeof(struct virtio_net_hdr) == 10u,
               "legacy virtio net header size without mergeable buffers");

int virtio_net_init(void);
int virtio_net_is_ready(void);
int virtio_net_get_mac(uint8_t mac[VIRTIO_NET_MAC_SIZE]);
int virtio_net_send_frame(const uint8_t *frame, size_t frame_length);
/* Returns 1 when the bounded poll completed without an RX packet. */
int virtio_net_poll_receive(uint8_t *frame, size_t capacity,
                            size_t *frame_length);

#endif
