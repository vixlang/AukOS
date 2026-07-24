#include "include/aukos/net_packets.h"

#include <limits.h>

#define ETHERNET_DESTINATION_OFFSET 0u
#define ETHERNET_SOURCE_OFFSET 6u
#define ETHERNET_TYPE_OFFSET 12u

#define ARP_HARDWARE_ETHERNET 1u
#define ARP_PROTOCOL_IPV4 NET_ETHERTYPE_IPV4
#define ARP_OPERATION_REQUEST 1u
#define ARP_OPERATION_REPLY 2u
#define ARP_HARDWARE_OFFSET 0u
#define ARP_PROTOCOL_OFFSET 2u
#define ARP_HARDWARE_LENGTH_OFFSET 4u
#define ARP_PROTOCOL_LENGTH_OFFSET 5u
#define ARP_OPERATION_OFFSET 6u
#define ARP_SENDER_HARDWARE_OFFSET 8u
#define ARP_SENDER_PROTOCOL_OFFSET 14u
#define ARP_TARGET_HARDWARE_OFFSET 18u
#define ARP_TARGET_PROTOCOL_OFFSET 24u

#define IPV4_VERSION_IHL_OFFSET 0u
#define IPV4_DSCP_ECN_OFFSET 1u
#define IPV4_TOTAL_LENGTH_OFFSET 2u
#define IPV4_IDENTIFICATION_OFFSET 4u
#define IPV4_FLAGS_FRAGMENT_OFFSET 6u
#define IPV4_TTL_OFFSET 8u
#define IPV4_PROTOCOL_OFFSET 9u
#define IPV4_CHECKSUM_OFFSET 10u
#define IPV4_SOURCE_OFFSET 12u
#define IPV4_DESTINATION_OFFSET 16u
#define IPV4_VERSION_IHL 0x45u
#define IPV4_FLAG_DF 0x4000u
#define IPV4_PROTOCOL_ICMP 1u
#define IPV4_PROTOCOL_UDP 17u

#define ICMP_TYPE_OFFSET 0u
#define ICMP_CODE_OFFSET 1u
#define ICMP_CHECKSUM_OFFSET 2u
#define ICMP_IDENTIFIER_OFFSET 4u
#define ICMP_SEQUENCE_OFFSET 6u
#define ICMP_ECHO_REPLY 0u
#define ICMP_ECHO_REQUEST 8u

#define UDP_SOURCE_PORT_OFFSET 0u
#define UDP_DESTINATION_PORT_OFFSET 2u
#define UDP_LENGTH_OFFSET 4u
#define UDP_CHECKSUM_OFFSET 6u

static void bytes_copy(uint8_t *destination, const uint8_t *source, size_t size)
{
    for (size_t index = 0; index < size; index++) {
        destination[index] = source[index];
    }
}

static void bytes_fill(uint8_t *destination, uint8_t value, size_t size)
{
    for (size_t index = 0; index < size; index++) {
        destination[index] = value;
    }
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

static int address_is_value(const uint8_t *address, uint8_t value)
{
    for (size_t index = 0; index < NET_MAC_SIZE; index++) {
        if (address[index] != value) {
            return 0;
        }
    }
    return 1;
}

uint16_t net_load_be16(const uint8_t *source)
{
    return (uint16_t)((uint16_t)source[0] << 8) | source[1];
}

uint32_t net_load_be32(const uint8_t *source)
{
    return ((uint32_t)source[0] << 24) | ((uint32_t)source[1] << 16) |
           ((uint32_t)source[2] << 8) | source[3];
}

void net_store_be16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value >> 8);
    destination[1] = (uint8_t)value;
}

void net_store_be32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value >> 24);
    destination[1] = (uint8_t)(value >> 16);
    destination[2] = (uint8_t)(value >> 8);
    destination[3] = (uint8_t)value;
}

static uint32_t checksum_sum(const uint8_t *data, size_t length)
{
    uint32_t sum = 0u;

    while (length >= 2u) {
        sum += net_load_be16(data);
        data += 2u;
        length -= 2u;
    }
    if (length != 0u) {
        sum += (uint32_t)data[0] << 8;
    }
    while ((sum >> 16) != 0u) {
        sum = (sum & 0xffffu) + (sum >> 16);
    }
    return sum;
}

static uint32_t checksum_add(uint32_t sum, const uint8_t *data, size_t length)
{
    while (length >= 2u) {
        sum += net_load_be16(data);
        data += 2u;
        length -= 2u;
    }
    if (length != 0u) {
        sum += (uint32_t)data[0] << 8;
    }
    while ((sum >> 16) != 0u) {
        sum = (sum & 0xffffu) + (sum >> 16);
    }
    return sum;
}

static uint16_t udp_checksum(const uint8_t source_ip[NET_IPV4_ADDRESS_SIZE],
                             const uint8_t destination_ip[NET_IPV4_ADDRESS_SIZE],
                             const uint8_t *udp, size_t udp_length)
{
    uint32_t sum = 0u;

    sum = checksum_add(sum, source_ip, NET_IPV4_ADDRESS_SIZE);
    sum = checksum_add(sum, destination_ip, NET_IPV4_ADDRESS_SIZE);
    sum += IPV4_PROTOCOL_UDP;
    sum += (uint16_t)udp_length;
    sum = checksum_add(sum, udp, udp_length);
    while ((sum >> 16) != 0u) {
        sum = (sum & 0xffffu) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

uint16_t net_checksum(const uint8_t *data, size_t length)
{
    if (!data && length != 0u) {
        return 0u;
    }
    return (uint16_t)~checksum_sum(data, length);
}

int net_checksum_valid(const uint8_t *data, size_t length)
{
    if (!data && length != 0u) {
        return 0;
    }
    return checksum_sum(data, length) == 0xffffu;
}

int net_parse_ethernet(const uint8_t *frame, size_t frame_length,
                       const uint8_t local_mac[NET_MAC_SIZE],
                       int allow_broadcast,
                       struct net_ethernet_view *view)
{
    int broadcast;

    if (!frame || !local_mac || !view ||
        frame_length < NET_ETHERNET_HEADER_SIZE ||
        (uintptr_t)frame > UINTPTR_MAX - frame_length) {
        return -1;
    }
    broadcast = address_is_value(frame + ETHERNET_DESTINATION_OFFSET, 0xffu);
    if (!bytes_equal(frame + ETHERNET_DESTINATION_OFFSET, local_mac,
                     NET_MAC_SIZE) && (!allow_broadcast || !broadcast)) {
        return -1;
    }
    view->source = frame + ETHERNET_SOURCE_OFFSET;
    view->payload = frame + NET_ETHERNET_HEADER_SIZE;
    view->payload_length = frame_length - NET_ETHERNET_HEADER_SIZE;
    view->ether_type = net_load_be16(frame + ETHERNET_TYPE_OFFSET);
    view->destination_is_broadcast = broadcast;
    return 0;
}

int net_build_arp_request(uint8_t *frame, size_t capacity,
                          const uint8_t local_mac[NET_MAC_SIZE],
                          const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE],
                          const uint8_t target_ip[NET_IPV4_ADDRESS_SIZE],
                          size_t *frame_length)
{
    uint8_t *arp;

    if (!frame || !local_mac || !local_ip || !target_ip || !frame_length ||
        capacity < NET_ETHERNET_MIN_FRAME_SIZE ||
        (uintptr_t)frame > UINTPTR_MAX - NET_ETHERNET_MIN_FRAME_SIZE) {
        return -1;
    }

    bytes_fill(frame, 0u, NET_ETHERNET_MIN_FRAME_SIZE);
    bytes_fill(frame + ETHERNET_DESTINATION_OFFSET, 0xffu, NET_MAC_SIZE);
    bytes_copy(frame + ETHERNET_SOURCE_OFFSET, local_mac, NET_MAC_SIZE);
    net_store_be16(frame + ETHERNET_TYPE_OFFSET, NET_ETHERTYPE_ARP);

    arp = frame + NET_ETHERNET_HEADER_SIZE;
    net_store_be16(arp + ARP_HARDWARE_OFFSET, ARP_HARDWARE_ETHERNET);
    net_store_be16(arp + ARP_PROTOCOL_OFFSET, ARP_PROTOCOL_IPV4);
    arp[ARP_HARDWARE_LENGTH_OFFSET] = NET_MAC_SIZE;
    arp[ARP_PROTOCOL_LENGTH_OFFSET] = NET_IPV4_ADDRESS_SIZE;
    net_store_be16(arp + ARP_OPERATION_OFFSET, ARP_OPERATION_REQUEST);
    bytes_copy(arp + ARP_SENDER_HARDWARE_OFFSET, local_mac, NET_MAC_SIZE);
    bytes_copy(arp + ARP_SENDER_PROTOCOL_OFFSET, local_ip,
               NET_IPV4_ADDRESS_SIZE);
    bytes_copy(arp + ARP_TARGET_PROTOCOL_OFFSET, target_ip,
               NET_IPV4_ADDRESS_SIZE);
    *frame_length = NET_ETHERNET_MIN_FRAME_SIZE;
    return 0;
}

int net_parse_arp_reply(
    const uint8_t *frame, size_t frame_length,
    const uint8_t local_mac[NET_MAC_SIZE],
    const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE],
    const uint8_t sender_ip[NET_IPV4_ADDRESS_SIZE],
    uint8_t sender_mac[NET_MAC_SIZE])
{
    struct net_ethernet_view ethernet;
    const uint8_t *arp;
    const uint8_t *arp_sender_hardware;

    if (!local_ip || !sender_ip || !sender_mac ||
        net_parse_ethernet(frame, frame_length, local_mac, 0, &ethernet) != 0 ||
        ethernet.ether_type != NET_ETHERTYPE_ARP ||
        ethernet.payload_length < NET_ARP_PACKET_SIZE) {
        return -1;
    }
    arp = ethernet.payload;
    arp_sender_hardware = arp + ARP_SENDER_HARDWARE_OFFSET;
    if (net_load_be16(arp + ARP_HARDWARE_OFFSET) != ARP_HARDWARE_ETHERNET ||
        net_load_be16(arp + ARP_PROTOCOL_OFFSET) != ARP_PROTOCOL_IPV4 ||
        arp[ARP_HARDWARE_LENGTH_OFFSET] != NET_MAC_SIZE ||
        arp[ARP_PROTOCOL_LENGTH_OFFSET] != NET_IPV4_ADDRESS_SIZE ||
        net_load_be16(arp + ARP_OPERATION_OFFSET) != ARP_OPERATION_REPLY ||
        !bytes_equal(arp + ARP_SENDER_PROTOCOL_OFFSET, sender_ip,
                     NET_IPV4_ADDRESS_SIZE) ||
        !bytes_equal(arp + ARP_TARGET_PROTOCOL_OFFSET, local_ip,
                     NET_IPV4_ADDRESS_SIZE) ||
        !bytes_equal(arp + ARP_TARGET_HARDWARE_OFFSET, local_mac,
                     NET_MAC_SIZE) ||
        !bytes_equal(ethernet.source, arp_sender_hardware, NET_MAC_SIZE) ||
        address_is_value(ethernet.source, 0u) ||
        address_is_value(ethernet.source, 0xffu) ||
        bytes_equal(ethernet.source, local_mac, NET_MAC_SIZE)) {
        return -1;
    }
    bytes_copy(sender_mac, arp_sender_hardware, NET_MAC_SIZE);
    return 0;
}

int net_build_icmp_echo_request(
    uint8_t *frame, size_t capacity,
    const uint8_t local_mac[NET_MAC_SIZE],
    const uint8_t destination_mac[NET_MAC_SIZE],
    const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE],
    const uint8_t destination_ip[NET_IPV4_ADDRESS_SIZE],
    uint16_t ip_identification, uint16_t identifier, uint16_t sequence,
    const uint8_t *payload, size_t payload_length, size_t *frame_length)
{
    size_t icmp_length;
    size_t ipv4_length;
    size_t wire_length;
    uint8_t *ipv4;
    uint8_t *icmp;

    if (!frame || !local_mac || !destination_mac || !local_ip ||
        !destination_ip || !frame_length || (!payload && payload_length != 0u) ||
        ip_identification == 0u || identifier == 0u ||
        payload_length > SIZE_MAX - NET_ICMP_ECHO_HEADER_SIZE ||
        (payload && (uintptr_t)payload > UINTPTR_MAX - payload_length)) {
        return -1;
    }
    icmp_length = NET_ICMP_ECHO_HEADER_SIZE + payload_length;
    if (icmp_length > UINT16_MAX - NET_IPV4_HEADER_SIZE) {
        return -1;
    }
    ipv4_length = NET_IPV4_HEADER_SIZE + icmp_length;
    if (ipv4_length > SIZE_MAX - NET_ETHERNET_HEADER_SIZE) {
        return -1;
    }
    wire_length = NET_ETHERNET_HEADER_SIZE + ipv4_length;
    if (wire_length < NET_ETHERNET_MIN_FRAME_SIZE) {
        wire_length = NET_ETHERNET_MIN_FRAME_SIZE;
    }
    if (wire_length > capacity || wire_length > NET_ETHERNET_MAX_FRAME_SIZE ||
        (uintptr_t)frame > UINTPTR_MAX - wire_length) {
        return -1;
    }

    bytes_fill(frame, 0u, wire_length);
    bytes_copy(frame + ETHERNET_DESTINATION_OFFSET, destination_mac,
               NET_MAC_SIZE);
    bytes_copy(frame + ETHERNET_SOURCE_OFFSET, local_mac, NET_MAC_SIZE);
    net_store_be16(frame + ETHERNET_TYPE_OFFSET, NET_ETHERTYPE_IPV4);

    ipv4 = frame + NET_ETHERNET_HEADER_SIZE;
    ipv4[IPV4_VERSION_IHL_OFFSET] = IPV4_VERSION_IHL;
    ipv4[IPV4_DSCP_ECN_OFFSET] = 0u;
    net_store_be16(ipv4 + IPV4_TOTAL_LENGTH_OFFSET, (uint16_t)ipv4_length);
    net_store_be16(ipv4 + IPV4_IDENTIFICATION_OFFSET, ip_identification);
    net_store_be16(ipv4 + IPV4_FLAGS_FRAGMENT_OFFSET, IPV4_FLAG_DF);
    ipv4[IPV4_TTL_OFFSET] = 64u;
    ipv4[IPV4_PROTOCOL_OFFSET] = IPV4_PROTOCOL_ICMP;
    net_store_be16(ipv4 + IPV4_CHECKSUM_OFFSET, 0u);
    bytes_copy(ipv4 + IPV4_SOURCE_OFFSET, local_ip, NET_IPV4_ADDRESS_SIZE);
    bytes_copy(ipv4 + IPV4_DESTINATION_OFFSET, destination_ip,
               NET_IPV4_ADDRESS_SIZE);
    net_store_be16(ipv4 + IPV4_CHECKSUM_OFFSET,
                   net_checksum(ipv4, NET_IPV4_HEADER_SIZE));

    icmp = ipv4 + NET_IPV4_HEADER_SIZE;
    icmp[ICMP_TYPE_OFFSET] = ICMP_ECHO_REQUEST;
    icmp[ICMP_CODE_OFFSET] = 0u;
    net_store_be16(icmp + ICMP_CHECKSUM_OFFSET, 0u);
    net_store_be16(icmp + ICMP_IDENTIFIER_OFFSET, identifier);
    net_store_be16(icmp + ICMP_SEQUENCE_OFFSET, sequence);
    bytes_copy(icmp + NET_ICMP_ECHO_HEADER_SIZE, payload, payload_length);
    net_store_be16(icmp + ICMP_CHECKSUM_OFFSET,
                   net_checksum(icmp, icmp_length));
    *frame_length = wire_length;
    return 0;
}

int net_validate_icmp_echo_reply(
    const uint8_t *frame, size_t frame_length,
    const uint8_t local_mac[NET_MAC_SIZE],
    const uint8_t source_mac[NET_MAC_SIZE],
    const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE],
    const uint8_t source_ip[NET_IPV4_ADDRESS_SIZE],
    uint16_t identifier, uint16_t sequence,
    const uint8_t *payload, size_t payload_length)
{
    struct net_ethernet_view ethernet;
    const uint8_t *ipv4;
    const uint8_t *icmp;
    size_t ipv4_header_length;
    size_t ipv4_total_length;
    size_t icmp_length;

    if (!source_mac || !local_ip || !source_ip ||
        (!payload && payload_length != 0u) ||
        net_parse_ethernet(frame, frame_length, local_mac, 0, &ethernet) != 0 ||
        ethernet.ether_type != NET_ETHERTYPE_IPV4 ||
        !bytes_equal(ethernet.source, source_mac, NET_MAC_SIZE) ||
        ethernet.payload_length < NET_IPV4_HEADER_SIZE) {
        return -1;
    }
    ipv4 = ethernet.payload;
    if ((ipv4[IPV4_VERSION_IHL_OFFSET] >> 4) != 4u ||
        (ipv4[IPV4_VERSION_IHL_OFFSET] & 0x0fu) != 5u) {
        return -1;
    }
    ipv4_header_length =
        (size_t)(ipv4[IPV4_VERSION_IHL_OFFSET] & 0x0fu) * 4u;
    ipv4_total_length = net_load_be16(ipv4 + IPV4_TOTAL_LENGTH_OFFSET);
    if (ipv4_total_length < ipv4_header_length + NET_ICMP_ECHO_HEADER_SIZE ||
        ipv4_total_length > ethernet.payload_length ||
        !net_checksum_valid(ipv4, ipv4_header_length) ||
        (net_load_be16(ipv4 + IPV4_FLAGS_FRAGMENT_OFFSET) & 0xbfffu) != 0u ||
        ipv4[IPV4_PROTOCOL_OFFSET] != IPV4_PROTOCOL_ICMP ||
        !bytes_equal(ipv4 + IPV4_SOURCE_OFFSET, source_ip,
                     NET_IPV4_ADDRESS_SIZE) ||
        !bytes_equal(ipv4 + IPV4_DESTINATION_OFFSET, local_ip,
                     NET_IPV4_ADDRESS_SIZE)) {
        return -1;
    }

    icmp = ipv4 + ipv4_header_length;
    icmp_length = ipv4_total_length - ipv4_header_length;
    if (icmp_length != NET_ICMP_ECHO_HEADER_SIZE + payload_length ||
        icmp[ICMP_TYPE_OFFSET] != ICMP_ECHO_REPLY ||
        icmp[ICMP_CODE_OFFSET] != 0u ||
        !net_checksum_valid(icmp, icmp_length) ||
        net_load_be16(icmp + ICMP_IDENTIFIER_OFFSET) != identifier ||
        net_load_be16(icmp + ICMP_SEQUENCE_OFFSET) != sequence ||
        !bytes_equal(icmp + NET_ICMP_ECHO_HEADER_SIZE, payload,
                     payload_length)) {
        return -1;
    }
    return 0;
}

int net_build_udp_datagram(
    uint8_t *frame, size_t capacity,
    const uint8_t local_mac[NET_MAC_SIZE],
    const uint8_t destination_mac[NET_MAC_SIZE],
    const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE],
    const uint8_t destination_ip[NET_IPV4_ADDRESS_SIZE],
    uint16_t ip_identification, uint16_t source_port,
    uint16_t destination_port, const uint8_t *payload,
    size_t payload_length, size_t *frame_length)
{
    size_t udp_length;
    size_t ipv4_length;
    size_t wire_length;
    uint16_t checksum;
    uint8_t *ipv4;
    uint8_t *udp;

    if (!frame || !local_mac || !destination_mac || !local_ip ||
        !destination_ip || !frame_length || source_port == 0u ||
        destination_port == 0u || (!payload && payload_length != 0u) ||
        payload_length > NET_UDP_MAX_PAYLOAD ||
        (payload && (uintptr_t)payload > UINTPTR_MAX - payload_length)) {
        return -1;
    }
    udp_length = NET_UDP_HEADER_SIZE + payload_length;
    ipv4_length = NET_IPV4_HEADER_SIZE + udp_length;
    wire_length = NET_ETHERNET_HEADER_SIZE + ipv4_length;
    if (wire_length < NET_ETHERNET_MIN_FRAME_SIZE) {
        wire_length = NET_ETHERNET_MIN_FRAME_SIZE;
    }
    if (wire_length > capacity || wire_length > NET_ETHERNET_MAX_FRAME_SIZE ||
        (uintptr_t)frame > UINTPTR_MAX - wire_length) {
        return -1;
    }

    bytes_fill(frame, 0u, wire_length);
    bytes_copy(frame + ETHERNET_DESTINATION_OFFSET, destination_mac,
               NET_MAC_SIZE);
    bytes_copy(frame + ETHERNET_SOURCE_OFFSET, local_mac, NET_MAC_SIZE);
    net_store_be16(frame + ETHERNET_TYPE_OFFSET, NET_ETHERTYPE_IPV4);

    ipv4 = frame + NET_ETHERNET_HEADER_SIZE;
    ipv4[IPV4_VERSION_IHL_OFFSET] = IPV4_VERSION_IHL;
    ipv4[IPV4_DSCP_ECN_OFFSET] = 0u;
    net_store_be16(ipv4 + IPV4_TOTAL_LENGTH_OFFSET, (uint16_t)ipv4_length);
    net_store_be16(ipv4 + IPV4_IDENTIFICATION_OFFSET, ip_identification);
    net_store_be16(ipv4 + IPV4_FLAGS_FRAGMENT_OFFSET, IPV4_FLAG_DF);
    ipv4[IPV4_TTL_OFFSET] = 64u;
    ipv4[IPV4_PROTOCOL_OFFSET] = IPV4_PROTOCOL_UDP;
    net_store_be16(ipv4 + IPV4_CHECKSUM_OFFSET, 0u);
    bytes_copy(ipv4 + IPV4_SOURCE_OFFSET, local_ip, NET_IPV4_ADDRESS_SIZE);
    bytes_copy(ipv4 + IPV4_DESTINATION_OFFSET, destination_ip,
               NET_IPV4_ADDRESS_SIZE);
    net_store_be16(ipv4 + IPV4_CHECKSUM_OFFSET,
                   net_checksum(ipv4, NET_IPV4_HEADER_SIZE));

    udp = ipv4 + NET_IPV4_HEADER_SIZE;
    net_store_be16(udp + UDP_SOURCE_PORT_OFFSET, source_port);
    net_store_be16(udp + UDP_DESTINATION_PORT_OFFSET, destination_port);
    net_store_be16(udp + UDP_LENGTH_OFFSET, (uint16_t)udp_length);
    net_store_be16(udp + UDP_CHECKSUM_OFFSET, 0u);
    bytes_copy(udp + NET_UDP_HEADER_SIZE, payload, payload_length);
    checksum = udp_checksum(local_ip, destination_ip, udp, udp_length);
    net_store_be16(udp + UDP_CHECKSUM_OFFSET,
                   checksum == 0u ? 0xffffu : checksum);
    *frame_length = wire_length;
    return 0;
}

int net_parse_udp_datagram(
    const uint8_t *frame, size_t frame_length,
    const uint8_t destination_mac[NET_MAC_SIZE],
    const uint8_t expected_source_mac[NET_MAC_SIZE],
    const uint8_t destination_ip[NET_IPV4_ADDRESS_SIZE],
    const uint8_t expected_source_ip[NET_IPV4_ADDRESS_SIZE],
    uint16_t expected_destination_port, uint16_t expected_source_port,
    struct net_udp_view *view)
{
    struct net_ethernet_view ethernet;
    const uint8_t *ipv4;
    const uint8_t *udp;
    size_t ipv4_header_length;
    size_t ipv4_total_length;
    size_t udp_length;
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t checksum;

    if (!destination_ip || !view ||
        net_parse_ethernet(frame, frame_length, destination_mac, 0,
                           &ethernet) != 0 ||
        ethernet.ether_type != NET_ETHERTYPE_IPV4 ||
        (expected_source_mac &&
         !bytes_equal(ethernet.source, expected_source_mac, NET_MAC_SIZE)) ||
        ethernet.payload_length < NET_IPV4_HEADER_SIZE) {
        return -1;
    }
    ipv4 = ethernet.payload;
    if ((ipv4[IPV4_VERSION_IHL_OFFSET] >> 4) != 4u ||
        (ipv4[IPV4_VERSION_IHL_OFFSET] & 0x0fu) != 5u) {
        return -1;
    }
    ipv4_header_length =
        (size_t)(ipv4[IPV4_VERSION_IHL_OFFSET] & 0x0fu) * 4u;
    ipv4_total_length = net_load_be16(ipv4 + IPV4_TOTAL_LENGTH_OFFSET);
    if (ipv4_total_length < ipv4_header_length + NET_UDP_HEADER_SIZE ||
        ipv4_total_length > ethernet.payload_length ||
        !net_checksum_valid(ipv4, ipv4_header_length) ||
        (net_load_be16(ipv4 + IPV4_FLAGS_FRAGMENT_OFFSET) & 0xbfffu) != 0u ||
        ipv4[IPV4_PROTOCOL_OFFSET] != IPV4_PROTOCOL_UDP ||
        !bytes_equal(ipv4 + IPV4_DESTINATION_OFFSET, destination_ip,
                     NET_IPV4_ADDRESS_SIZE) ||
        (expected_source_ip &&
         !bytes_equal(ipv4 + IPV4_SOURCE_OFFSET, expected_source_ip,
                      NET_IPV4_ADDRESS_SIZE))) {
        return -1;
    }

    udp = ipv4 + ipv4_header_length;
    udp_length = net_load_be16(udp + UDP_LENGTH_OFFSET);
    source_port = net_load_be16(udp + UDP_SOURCE_PORT_OFFSET);
    destination_port = net_load_be16(udp + UDP_DESTINATION_PORT_OFFSET);
    checksum = net_load_be16(udp + UDP_CHECKSUM_OFFSET);
    if (udp_length < NET_UDP_HEADER_SIZE ||
        udp_length != ipv4_total_length - ipv4_header_length ||
        source_port == 0u || destination_port == 0u ||
        (expected_source_port != 0u && source_port != expected_source_port) ||
        (expected_destination_port != 0u &&
         destination_port != expected_destination_port) ||
        (checksum != 0u &&
         udp_checksum(ipv4 + IPV4_SOURCE_OFFSET,
                      ipv4 + IPV4_DESTINATION_OFFSET, udp, udp_length) != 0u)) {
        return -1;
    }

    view->source_ip = ipv4 + IPV4_SOURCE_OFFSET;
    view->destination_ip = ipv4 + IPV4_DESTINATION_OFFSET;
    view->source_port = source_port;
    view->destination_port = destination_port;
    view->payload = udp + NET_UDP_HEADER_SIZE;
    view->payload_length = udp_length - NET_UDP_HEADER_SIZE;
    view->checksum_present = checksum != 0u;
    return 0;
}
