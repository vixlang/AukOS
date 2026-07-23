#ifdef AUKOS_ETHERNET_H
#define AUKOS_ETHERNET_H

#include <stdint.h>
#include <stddef.h>

#define ETHER_MAC_SIZE 6u

#define ETHERNET_MAX_FREAME 1514u

int ethernet_init(void);
int ethernet_is_ready(void);
int ethernet_get_mac(uint8_t mac[ETHER_MAC_SIZE]);
int ethernet_send_frame(const)
