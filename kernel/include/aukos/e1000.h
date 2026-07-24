#ifndef AUKOS_E1000_H
#define AUKOS_E1000_H

#include <stddef.h>
#include <stdint.h>

#define E1000_MAC_SIZE 6u
#define E1000_MIN_FRAME_SIZE 60u
#define E1000_MAX_FRAME_SIZE 1514u
#define E1000_RX_DESCRIPTOR_COUNT 32u
#define E1000_TX_DESCRIPTOR_COUNT 8u
#define E1000_RX_BUFFER_SIZE 2048u

struct e1000_rx_descriptor {
    uint64_t address;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

struct e1000_tx_descriptor {
    uint64_t address;
    uint16_t length;
    uint8_t checksum_offset;
    uint8_t command;
    uint8_t status;
    uint8_t checksum_start;
    uint16_t special;
} __attribute__((packed));

_Static_assert(sizeof(struct e1000_rx_descriptor) == 16u,
               "e1000 RX descriptor size");
_Static_assert(sizeof(struct e1000_tx_descriptor) == 16u,
               "e1000 TX descriptor size");

int e1000_init(void);
int e1000_is_ready(void);
int e1000_get_mac(uint8_t mac[E1000_MAC_SIZE]);
int e1000_send_frame(const uint8_t *frame, size_t frame_length);
int e1000_poll_receive(uint8_t *frame, size_t capacity,
                       size_t *frame_length);

#endif
