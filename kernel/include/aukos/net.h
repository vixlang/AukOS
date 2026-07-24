#ifndef AUKOS_NET_H
#define AUKOS_NET_H

#include <stddef.h>
#include <stdint.h>

struct udp_socket;

struct net_status {
    uint32_t initialized;
    uint32_t ethernet_ready;
    uint32_t ipv4_ready;
    uint32_t arp_ready;
    uint32_t icmp_ready;
    uint32_t udp_ready;
    uint32_t tcp_ready;
};

void net_init(void);
void net_run_selftest(void);
struct net_status net_get_status(void);
struct udp_socket *net_udp_socket_create(void);
void net_udp_socket_close(struct udp_socket *socket);
int net_udp_socket_bind(struct udp_socket *socket,
                        const uint8_t local_ip[4], uint16_t local_port);
int net_udp_socket_sendto(struct udp_socket *socket, const uint8_t *payload,
                          size_t payload_length,
                          const uint8_t destination_ip[4],
                          uint16_t destination_port, size_t *sent_length);
int net_udp_socket_recvfrom(struct udp_socket *socket, uint8_t *buffer,
                            size_t capacity, uint8_t source_ip[4],
                            uint16_t *source_port, size_t *received_length);

#endif
