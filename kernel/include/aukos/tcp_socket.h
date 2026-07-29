#ifndef AUKOS_TCP_SOCKET_H
#define AUKOS_TCP_SOCKET_H

#include "net_packets.h"

#include <stddef.h>
#include <stdint.h>

#define TCP_SOCKET_LIMIT 8u
#define TCP_SOCKET_RX_CAPACITY 8192u

enum tcp_socket_state {
    TCP_SOCKET_CLOSED = 0,
    TCP_SOCKET_SYN_SENT,
    TCP_SOCKET_ESTABLISHED,
    TCP_SOCKET_CLOSE_WAIT,
    TCP_SOCKET_RESET,
};

struct tcp_socket {
    int allocated;
    enum tcp_socket_state state;
    uint8_t local_ip[NET_IPV4_ADDRESS_SIZE];
    uint8_t remote_ip[NET_IPV4_ADDRESS_SIZE];
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t send_unacknowledged;
    uint32_t send_next;
    uint32_t receive_next;
    size_t rx_head;
    size_t rx_count;
    uint8_t rx[TCP_SOCKET_RX_CAPACITY];
};

void tcp_socket_table_init(void);
struct tcp_socket *tcp_socket_allocate(void);
void tcp_socket_free(struct tcp_socket *socket);
struct tcp_socket *tcp_socket_find(uint16_t local_port,
                                   const uint8_t remote_ip[4],
                                   uint16_t remote_port);
int tcp_socket_begin_connect(struct tcp_socket *socket,
                             const uint8_t local_ip[4], uint16_t local_port,
                             const uint8_t remote_ip[4], uint16_t remote_port,
                             uint32_t initial_sequence);
int tcp_socket_handle_segment(struct tcp_socket *socket,
                              const struct net_tcp_view *view,
                              int *ack_required);
size_t tcp_socket_read(struct tcp_socket *socket, uint8_t *buffer,
                       size_t capacity);
int tcp_socket_eof(const struct tcp_socket *socket);

#endif
