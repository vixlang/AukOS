#ifndef AUKOS_VIRTIO_BLK_H
#define AUKOS_VIRTIO_BLK_H

#include <aukos/virtio_legacy.h>

#define VIRTIO_BLK_FIXTURE_SECTORS 128u

int virtio_blk_init(void);

#endif
