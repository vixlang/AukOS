#include "include/aukos/net.h"

#include "include/aukos/ethernet.h"
#include "include/aukos/log.h"
#include "include/aukos/net_packets.h"
#include "include/aukos/tcp_socket.h"
#include "include/aukos/timer.h"
#include "include/aukos/udp_socket.h"

#include <stddef.h>
#include <stdint.h>

#define NET_RX_PACKET_LIMIT 8u
#define NET_IP_IDENTIFICATION 0x4f53u
#define NET_ICMP_IDENTIFIER 0xa905u
#define NET_ICMP_SEQUENCE 1u
#define NET_UDP_RX_POLL_LIMIT 8u
#define NET_TCP_POLL_LIMIT 16u
#define NET_TCP_RETRY_LIMIT 3u

struct net_arp_entry {
    uint8_t ipv4[NET_IPV4_ADDRESS_SIZE];
    uint8_t mac[NET_MAC_SIZE];
    int valid;
};

static const uint8_t configured_local_ip[NET_IPV4_ADDRESS_SIZE] =
    {10u, 0u, 2u, 15u};
static const uint8_t configured_gateway_ip[NET_IPV4_ADDRESS_SIZE] =
    {10u, 0u, 2u, 2u};
static const uint8_t echo_payload[] = "AukOS ICMP echo";

static struct net_status status;
static struct net_arp_entry gateway_entry;
static uint16_t udp_ip_identification = 0x6000u;
static uint16_t tcp_ip_identification = 0x7000u;
static uint16_t icmp_ip_identification = 0x5000u;
static uint16_t tcp_ephemeral_port = 49152u;
static uint32_t tcp_initial_sequence = 0x41554b00u;

static int poll_and_demux(void);

static void bytes_copy(uint8_t *destination, const uint8_t *source, size_t size)
{
    for (size_t index = 0; index < size; index++) {
        destination[index] = source[index];
    }
}

static void bytes_zero(uint8_t *destination, size_t size)
{
    for (size_t index = 0; index < size; index++) {
        destination[index] = 0u;
    }
}

void net_init(void)
{
    udp_socket_table_init();
    tcp_socket_table_init();
    status.initialized = 1;
    status.ethernet_ready = 0;
    status.ipv4_ready = 0;
    status.arp_ready = 0;
    status.icmp_ready = 0;
    status.udp_ready = 0;
    status.tcp_ready = 0;
    bytes_zero(gateway_entry.ipv4, sizeof(gateway_entry.ipv4));
    bytes_zero(gateway_entry.mac, sizeof(gateway_entry.mac));
    gateway_entry.valid = 0;
    udp_ip_identification = 0x6000u;
    tcp_ip_identification = 0x7000u;
    icmp_ip_identification = 0x5000u;
    tcp_ephemeral_port = 49152u;
    log_info("net: subsystem initialized");
}

struct net_status net_get_status(void)
{
    return status;
}

static int resolve_gateway(const uint8_t local_mac[NET_MAC_SIZE])
{
    uint8_t request[NET_ETHERNET_MIN_FRAME_SIZE];
    uint8_t reply[NET_ETHERNET_MAX_FRAME_SIZE];
    uint8_t learned_mac[NET_MAC_SIZE];
    struct net_ethernet_view ethernet;
    size_t request_length;
    size_t reply_length;

    gateway_entry.valid = 0;
    bytes_zero(gateway_entry.ipv4, sizeof(gateway_entry.ipv4));
    bytes_zero(gateway_entry.mac, sizeof(gateway_entry.mac));
    status.arp_ready = 0;

    if (net_build_arp_request(request, sizeof(request), local_mac,
                              configured_local_ip, configured_gateway_ip,
                              &request_length) != 0 ||
        ethernet_send_frame(request, request_length) != 0) {
        return -1;
    }
    for (uint32_t packet = 0; packet < NET_RX_PACKET_LIMIT; packet++) {
        if (ethernet_poll_receive(reply, sizeof(reply), &reply_length) != 0) {
            return -1;
        }
        if (net_parse_ethernet(reply, reply_length, local_mac, 1,
                               &ethernet) != 0 ||
            ethernet.ether_type != NET_ETHERTYPE_ARP) {
            continue;
        }
        if (net_parse_arp_reply(reply, reply_length, local_mac,
                                configured_local_ip, configured_gateway_ip,
                                learned_mac) == 0) {
            bytes_copy(gateway_entry.ipv4, configured_gateway_ip,
                       NET_IPV4_ADDRESS_SIZE);
            bytes_copy(gateway_entry.mac, learned_mac, NET_MAC_SIZE);
            gateway_entry.valid = 1;
            status.arp_ready = 1;
            log_info("net: gateway ARP entry learned");
            return 0;
        }
    }
    return -1;
}

static int echo_gateway(const uint8_t local_mac[NET_MAC_SIZE])
{
    uint8_t request[NET_ETHERNET_MAX_FRAME_SIZE];
    uint8_t reply[NET_ETHERNET_MAX_FRAME_SIZE];
    struct net_ethernet_view ethernet;
    size_t request_length;
    size_t reply_length;

    status.ipv4_ready = 0;
    status.icmp_ready = 0;
    if (!gateway_entry.valid ||
        net_build_icmp_echo_request(
            request, sizeof(request), local_mac, gateway_entry.mac,
            configured_local_ip, gateway_entry.ipv4,
            NET_IP_IDENTIFICATION, NET_ICMP_IDENTIFIER, NET_ICMP_SEQUENCE,
            echo_payload, sizeof(echo_payload) - 1u, &request_length) != 0 ||
        ethernet_send_frame(request, request_length) != 0) {
        return -1;
    }
    for (uint32_t packet = 0; packet < NET_RX_PACKET_LIMIT; packet++) {
        if (ethernet_poll_receive(reply, sizeof(reply), &reply_length) != 0) {
            return -1;
        }
        if (net_parse_ethernet(reply, reply_length, local_mac, 1,
                               &ethernet) != 0 ||
            ethernet.ether_type != NET_ETHERTYPE_IPV4) {
            continue;
        }
        if (net_validate_icmp_echo_reply(
                reply, reply_length, local_mac, gateway_entry.mac,
                configured_local_ip, gateway_entry.ipv4,
                NET_ICMP_IDENTIFIER, NET_ICMP_SEQUENCE,
                echo_payload, sizeof(echo_payload) - 1u) == 0) {
            status.ipv4_ready = 1;
            status.icmp_ready = 1;
            log_info("net: ICMP echo reply received from 10.0.2.2");
            return 0;
        }
    }
    return -1;
}

void net_run_selftest(void)
{
    uint8_t local_mac[NET_MAC_SIZE];

    status.ethernet_ready = 0;
    status.arp_ready = 0;
    status.ipv4_ready = 0;
    status.icmp_ready = 0;
    gateway_entry.valid = 0;
    bytes_zero(gateway_entry.ipv4, sizeof(gateway_entry.ipv4));
    bytes_zero(gateway_entry.mac, sizeof(gateway_entry.mac));
    if (!status.initialized || !ethernet_is_ready() ||
        ethernet_get_mac(local_mac) != 0) {
        log_error("net: Ethernet frame API unavailable");
        return;
    }
    status.ethernet_ready = 1;
    if (resolve_gateway(local_mac) != 0) {
        if (!ethernet_is_ready()) {
            status.ethernet_ready = 0;
        }
        log_error("net: gateway ARP resolution failed");
        return;
    }
    if (echo_gateway(local_mac) != 0) {
        if (!ethernet_is_ready()) {
            status.ethernet_ready = 0;
        }
        log_error("net: gateway IPv4/ICMP echo failed");
        return;
    }
    status.udp_ready = 1;
    status.tcp_ready = 1;
    log_info("net: gateway ARP/IPv4/ICMP self-test passed");
}

int net_icmp_echo(const uint8_t destination_ip[4], uint16_t sequence,
                  uint64_t *elapsed_ticks)
{
    static const uint8_t payload[] = "AukOS ping";
    uint8_t local_mac[NET_MAC_SIZE];
    uint8_t request[NET_ETHERNET_MAX_FRAME_SIZE];
    uint8_t reply[NET_ETHERNET_MAX_FRAME_SIZE];
    struct net_ethernet_view ethernet;
    size_t request_length;
    size_t reply_length;
    uint64_t started;

    if (!destination_ip || !elapsed_ticks || !status.icmp_ready ||
        !gateway_entry.valid || ethernet_get_mac(local_mac) != 0) {
        return -1;
    }

    started = timer_ticks();
    if (net_build_icmp_echo_request(
            request, sizeof(request), local_mac, gateway_entry.mac,
            configured_local_ip, destination_ip, icmp_ip_identification++,
            NET_ICMP_IDENTIFIER, sequence, payload, sizeof(payload) - 1u,
            &request_length) != 0 ||
        ethernet_send_frame(request, request_length) != 0) {
        return -1;
    }

    for (uint32_t packet = 0; packet < NET_RX_PACKET_LIMIT; packet++) {
        if (ethernet_poll_receive(reply, sizeof(reply), &reply_length) != 0) {
            return -1;
        }
        if (net_parse_ethernet(reply, reply_length, local_mac, 1,
                               &ethernet) != 0 ||
            ethernet.ether_type != NET_ETHERTYPE_IPV4) {
            continue;
        }
        if (net_validate_icmp_echo_reply(
                reply, reply_length, local_mac, gateway_entry.mac,
                configured_local_ip, destination_ip, NET_ICMP_IDENTIFIER,
                sequence, payload, sizeof(payload) - 1u) == 0) {
            *elapsed_ticks = timer_ticks() - started;
            return 0;
        }
    }
    return -1;
}

struct udp_socket *net_udp_socket_create(void)
{
    if (!status.udp_ready || !gateway_entry.valid ||
        !ethernet_is_ready()) {
        return 0;
    }
    return udp_socket_allocate();
}

void net_udp_socket_close(struct udp_socket *socket)
{
    udp_socket_free(socket);
}

static int bytes_are_zero(const uint8_t *value, size_t size)
{
    for (size_t index = 0; index < size; index++) {
        if (value[index] != 0u) {
            return 0;
        }
    }
    return 1;
}

static int bytes_equal(const uint8_t *left, const uint8_t *right, size_t size)
{
    for (size_t index = 0; index < size; index++) {
        if (left[index] != right[index]) {
            return 0;
        }
    }
    return 1;
}

int net_udp_socket_bind(struct udp_socket *socket,
                        const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE],
                        uint16_t local_port)
{
    if (!local_ip ||
        (!bytes_are_zero(local_ip, NET_IPV4_ADDRESS_SIZE) &&
         !bytes_equal(local_ip, configured_local_ip,
                      NET_IPV4_ADDRESS_SIZE))) {
        return -1;
    }
    return udp_socket_bind_local(socket, configured_local_ip, local_port);
}

int net_udp_socket_sendto(struct udp_socket *socket, const uint8_t *payload,
                          size_t payload_length,
                          const uint8_t destination_ip[NET_IPV4_ADDRESS_SIZE],
                          uint16_t destination_port, size_t *sent_length)
{
    uint8_t local_mac[NET_MAC_SIZE];
    uint8_t frame[NET_ETHERNET_MAX_FRAME_SIZE];
    size_t frame_length;

    if (!socket || !socket->allocated || !socket->bound || !destination_ip ||
        !sent_length || !gateway_entry.valid ||
        !bytes_equal(destination_ip, configured_gateway_ip,
                     NET_IPV4_ADDRESS_SIZE) ||
        ethernet_get_mac(local_mac) != 0 ||
        net_build_udp_datagram(
            frame, sizeof(frame), local_mac, gateway_entry.mac,
            configured_local_ip, destination_ip, udp_ip_identification++,
            socket->local_port, destination_port, payload, payload_length,
            &frame_length) != 0 ||
        ethernet_send_frame(frame, frame_length) != 0) {
        return -1;
    }
    *sent_length = payload_length;
    return 0;
}

int net_udp_socket_recvfrom(struct udp_socket *socket, uint8_t *buffer,
                            size_t capacity,
                            uint8_t source_ip[NET_IPV4_ADDRESS_SIZE],
                            uint16_t *source_port, size_t *received_length)
{
    if (!socket || !socket->allocated || !socket->bound || !buffer ||
        !source_ip || !source_port || !received_length) {
        return -1;
    }
    if (udp_socket_dequeue(socket, buffer, capacity, source_ip, source_port,
                           received_length) == 0) {
        return 0;
    }
    for (uint32_t attempt = 0; attempt < NET_UDP_RX_POLL_LIMIT; attempt++) {
        int poll_result = poll_and_demux();

        if (poll_result < 0) {
            return -1;
        }
        if (udp_socket_dequeue(socket, buffer, capacity, source_ip,
                               source_port, received_length) == 0) {
            return 0;
        }
    }
    return -1;
}

static int send_tcp_segment(struct tcp_socket *socket, uint32_t sequence,
                            uint8_t flags, const uint8_t *payload,
                            size_t payload_length)
{
    uint8_t local_mac[NET_MAC_SIZE];
    uint8_t frame[NET_ETHERNET_MAX_FRAME_SIZE];
    size_t frame_length;

    if (!socket || ethernet_get_mac(local_mac) != 0 ||
        net_build_tcp_segment(
            frame, sizeof(frame), local_mac, gateway_entry.mac,
            configured_local_ip, socket->remote_ip, tcp_ip_identification++,
            socket->local_port, socket->remote_port, sequence,
            socket->receive_next, flags, TCP_SOCKET_RX_CAPACITY,
            payload, payload_length, &frame_length) != 0) return -1;
    return ethernet_send_frame(frame, frame_length);
}

static int poll_and_demux(void)
{
    uint8_t local_mac[NET_MAC_SIZE];
    uint8_t frame[NET_ETHERNET_MAX_FRAME_SIZE];
    struct net_udp_view udp;
    struct net_tcp_view tcp;
    struct udp_socket *udp_destination;
    struct tcp_socket *tcp_destination;
    size_t frame_length;
    int ack_required;
    int result;

    if (ethernet_get_mac(local_mac) != 0) return -1;
    result = ethernet_poll_receive(frame, sizeof(frame), &frame_length);
    if (result != 0) return result;
    if (net_parse_udp_datagram(frame, frame_length, local_mac, gateway_entry.mac,
                               configured_local_ip, configured_gateway_ip,
                               0u, 0u, &udp) == 0) {
        udp_destination = udp_socket_find_bound(udp.destination_port);
        if (udp_destination) {
            (void)udp_socket_enqueue(udp_destination, udp.source_ip,
                                     udp.source_port, udp.payload,
                                     udp.payload_length);
        }
        return 0;
    }
    if (net_parse_tcp_segment(frame, frame_length, local_mac, gateway_entry.mac,
                              configured_local_ip, configured_gateway_ip,
                              0u, 0u, &tcp) != 0) return 0;
    tcp_destination = tcp_socket_find(tcp.destination_port, tcp.source_ip,
                                      tcp.source_port);
    if (!tcp_destination) return 0;
    result = tcp_socket_handle_segment(tcp_destination, &tcp, &ack_required);
    if (ack_required && send_tcp_segment(tcp_destination,
                                         tcp_destination->send_next,
                                         NET_TCP_FLAG_ACK, 0, 0u) != 0) return -1;
    return result;
}

struct tcp_socket *net_tcp_socket_create(void)
{
    if (!status.tcp_ready || !gateway_entry.valid || !ethernet_is_ready()) return 0;
    return tcp_socket_allocate();
}

int net_tcp_socket_connect(struct tcp_socket *socket,
                           const uint8_t remote_ip[4], uint16_t remote_port)
{
    uint16_t local_port;
    uint32_t initial_sequence;

    if (!socket || !remote_ip ||
        !bytes_equal(remote_ip, configured_gateway_ip, 4u) || remote_port == 0u)
        return -1;
    local_port = tcp_ephemeral_port++;
    if (tcp_ephemeral_port < 49152u) tcp_ephemeral_port = 49152u;
    initial_sequence = tcp_initial_sequence;
    tcp_initial_sequence += 0x101u;
    if (tcp_socket_begin_connect(socket, configured_local_ip, local_port,
                                 remote_ip, remote_port, initial_sequence) != 0)
        return -1;
    for (uint32_t retry = 0; retry < NET_TCP_RETRY_LIMIT; retry++) {
        if (send_tcp_segment(socket, initial_sequence, NET_TCP_FLAG_SYN,
                             0, 0u) != 0) return -1;
        for (uint32_t poll = 0; poll < NET_TCP_POLL_LIMIT; poll++) {
            (void)poll_and_demux();
            if (socket->state == TCP_SOCKET_ESTABLISHED) return 0;
            if (socket->state == TCP_SOCKET_RESET) return -1;
        }
    }
    return -1;
}

int net_tcp_socket_write(struct tcp_socket *socket, const uint8_t *buffer,
                         size_t length, size_t *written)
{
    size_t count;
    uint32_t sequence;

    if (!socket || (!buffer && length) || !written ||
        socket->state != TCP_SOCKET_ESTABLISHED) return -1;
    count = length < NET_TCP_MAX_PAYLOAD ? length : NET_TCP_MAX_PAYLOAD;
    if (count == 0u) { *written = 0u; return 0; }
    sequence = socket->send_next;
    socket->send_next += (uint32_t)count;
    for (uint32_t retry = 0; retry < NET_TCP_RETRY_LIMIT; retry++) {
        if (send_tcp_segment(socket, sequence,
                             NET_TCP_FLAG_ACK | NET_TCP_FLAG_PSH,
                             buffer, count) != 0) break;
        for (uint32_t poll = 0; poll < NET_TCP_POLL_LIMIT; poll++) {
            (void)poll_and_demux();
            if (socket->send_unacknowledged == socket->send_next) {
                *written = count;
                return 0;
            }
            if (socket->state == TCP_SOCKET_RESET) break;
        }
    }
    socket->send_next = sequence;
    return -1;
}

int net_tcp_socket_read(struct tcp_socket *socket, uint8_t *buffer,
                        size_t capacity, size_t *read_length)
{
    if (!socket || (!buffer && capacity) || !read_length) return -1;
    *read_length = tcp_socket_read(socket, buffer, capacity);
    if (*read_length || capacity == 0u || tcp_socket_eof(socket)) return 0;
    for (uint32_t poll = 0; poll < NET_TCP_POLL_LIMIT; poll++) {
        (void)poll_and_demux();
        *read_length = tcp_socket_read(socket, buffer, capacity);
        if (*read_length || tcp_socket_eof(socket)) return 0;
        if (socket->state == TCP_SOCKET_RESET) return -1;
    }
    return -1;
}

void net_tcp_socket_close(struct tcp_socket *socket)
{
    if (!socket) return;
    if (socket->state == TCP_SOCKET_ESTABLISHED) {
        (void)send_tcp_segment(socket, socket->send_next,
                               NET_TCP_FLAG_FIN | NET_TCP_FLAG_ACK, 0, 0u);
    }
    tcp_socket_free(socket);
}
