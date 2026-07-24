#ifndef AUKOS_UDP_SOCKET_H
#define AUKOS_UDP_SOCKET_H

#include "net_packets.h"

#include <stddef.h>
#include <stdint.h>

#define UDP_SOCKET_LIMIT 8u
#define UDP_SOCKET_QUEUE_LIMIT 4u

struct udp_datagram {
    uint8_t source_ip[NET_IPV4_ADDRESS_SIZE];
    uint16_t source_port;
    size_t length;
    uint8_t payload[NET_UDP_MAX_PAYLOAD];
};

struct udp_socket {
    int allocated;
    int bound;
    uint8_t local_ip[NET_IPV4_ADDRESS_SIZE];
    uint16_t local_port;
    size_t queue_head;
    size_t queue_count;
    struct udp_datagram queue[UDP_SOCKET_QUEUE_LIMIT];
};

void udp_socket_table_init(void);
struct udp_socket *udp_socket_allocate(void);
void udp_socket_free(struct udp_socket *socket);
int udp_socket_bind_local(struct udp_socket *socket,
                          const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE],
                          uint16_t local_port);
struct udp_socket *udp_socket_find_bound(uint16_t local_port);
int udp_socket_enqueue(struct udp_socket *socket,
                       const uint8_t source_ip[NET_IPV4_ADDRESS_SIZE],
                       uint16_t source_port, const uint8_t *payload,
                       size_t payload_length);
int udp_socket_dequeue(struct udp_socket *socket, uint8_t *buffer,
                       size_t capacity,
                       uint8_t source_ip[NET_IPV4_ADDRESS_SIZE],
                       uint16_t *source_port, size_t *copied_length);

#endif
