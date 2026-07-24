#ifndef AUKOS_ETHERNET_H
#define AUKOS_ETHERNET_H

#include <stddef.h>
#include <stdint.h>

#define ETHERNET_MAC_SIZE 6u
#define ETHERNET_HEADER_SIZE 14u
#define ETHERNET_MIN_FRAME_SIZE 60u
#define ETHERNET_MAX_FRAME_SIZE 1514u

int ethernet_init(void);
int ethernet_is_ready(void);
int ethernet_get_mac(uint8_t mac[ETHERNET_MAC_SIZE]);
int ethernet_send_frame(const uint8_t *frame, size_t frame_length);
/* Returns 1 when the bounded poll completed without an RX packet. */
int ethernet_poll_receive(uint8_t *frame, size_t capacity,
                          size_t *frame_length);

#endif
