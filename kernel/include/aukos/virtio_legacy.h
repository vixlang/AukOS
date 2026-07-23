#ifndef AUKOS_VIRTIO_LEGACY_H
#define AUKOS_VIRTIO_LEGACY_H

#include <stddef.h>
#include <stdint.h>

#define VIRTIO_LEGACY_QUEUE_ALIGN 4096u
#define VIRTIO_LEGACY_QUEUE_MAX 256u

#define VIRTQ_DESC_F_NEXT 1u
#define VIRTQ_DESC_F_WRITE 2u
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1u

struct virtio_legacy_desc {
    uint64_t address;
    uint32_t length;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct virtio_legacy_used_elem {
    uint32_t id;
    uint32_t length;
} __attribute__((packed));

_Static_assert(sizeof(struct virtio_legacy_desc) == 16u,
               "legacy descriptor size");
_Static_assert(sizeof(struct virtio_legacy_used_elem) == 8u,
               "legacy used element size");

static inline int virtio_legacy_queue_layout(uint16_t queue_size,
                                             size_t *used_offset,
                                             size_t *total_size)
{
    size_t descriptors;
    size_t available_end;
    size_t used;

    if (!used_offset || !total_size || queue_size == 0 ||
        queue_size > VIRTIO_LEGACY_QUEUE_MAX) {
        return -1;
    }
    descriptors = (size_t)queue_size * sizeof(struct virtio_legacy_desc);
    available_end = descriptors + 4u + (size_t)queue_size * 2u;
    used = (available_end + VIRTIO_LEGACY_QUEUE_ALIGN - 1u) &
           ~(VIRTIO_LEGACY_QUEUE_ALIGN - 1u);
    *used_offset = used;
    *total_size = used + 4u +
                  (size_t)queue_size * sizeof(struct virtio_legacy_used_elem);
    return 0;
}

#endif
