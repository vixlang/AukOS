#include "include/aukos/net.h"

#include "include/aukos/ethernet.h"
#include "include/aukos/log.h"
#include "include/aukos/net_packets.h"
#include "include/aukos/udp_socket.h"

#include <stddef.h>
#include <stdint.h>

#define NET_RX_PACKET_LIMIT 8u
#define NET_IP_IDENTIFICATION 0x4f53u
#define NET_ICMP_IDENTIFIER 0xa905u
#define NET_ICMP_SEQUENCE 1u
#define NET_UDP_RX_POLL_LIMIT 8u

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
    log_info("net: gateway ARP/IPv4/ICMP self-test passed");
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

static int poll_and_demux_udp(void)
{
    uint8_t local_mac[NET_MAC_SIZE];
    uint8_t frame[NET_ETHERNET_MAX_FRAME_SIZE];
    struct net_udp_view view;
    struct udp_socket *destination;
    size_t frame_length;
    int receive_result;

    if (ethernet_get_mac(local_mac) != 0) {
        return -1;
    }
    receive_result = ethernet_poll_receive(frame, sizeof(frame),
                                           &frame_length);
    if (receive_result != 0) {
        return receive_result;
    }
    if (net_parse_udp_datagram(
            frame, frame_length, local_mac, gateway_entry.mac,
            configured_local_ip, configured_gateway_ip, 0u, 0u,
            &view) != 0) {
        return 0;
    }
    destination = udp_socket_find_bound(view.destination_port);
    if (!destination) {
        return 0;
    }
    (void)udp_socket_enqueue(destination, view.source_ip, view.source_port,
                             view.payload, view.payload_length);
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
        int poll_result = poll_and_demux_udp();

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
