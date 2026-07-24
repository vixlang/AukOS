#ifndef AUKOS_NET_PACKETS_H
#define AUKOS_NET_PACKETS_H

#include <stddef.h>
#include <stdint.h>

#define NET_MAC_SIZE 6u
#define NET_IPV4_ADDRESS_SIZE 4u
#define NET_ETHERNET_HEADER_SIZE 14u
#define NET_ARP_PACKET_SIZE 28u
#define NET_ARP_FRAME_SIZE (NET_ETHERNET_HEADER_SIZE + NET_ARP_PACKET_SIZE)
#define NET_IPV4_HEADER_SIZE 20u
#define NET_ICMP_ECHO_HEADER_SIZE 8u
#define NET_UDP_HEADER_SIZE 8u
#define NET_ETHERNET_MIN_FRAME_SIZE 60u
#define NET_ETHERNET_MAX_FRAME_SIZE 1514u
#define NET_UDP_MAX_PAYLOAD (NET_ETHERNET_MAX_FRAME_SIZE - \
                             NET_ETHERNET_HEADER_SIZE - \
                             NET_IPV4_HEADER_SIZE - NET_UDP_HEADER_SIZE)

#define NET_ETHERTYPE_IPV4 0x0800u
#define NET_ETHERTYPE_ARP 0x0806u

struct net_ethernet_view {
    const uint8_t *source;
    const uint8_t *payload;
    size_t payload_length;
    uint16_t ether_type;
    int destination_is_broadcast;
};

struct net_udp_view {
    const uint8_t *source_ip;
    const uint8_t *destination_ip;
    const uint8_t *payload;
    size_t payload_length;
    uint16_t source_port;
    uint16_t destination_port;
    int checksum_present;
};

uint16_t net_load_be16(const uint8_t *source);
uint32_t net_load_be32(const uint8_t *source);
void net_store_be16(uint8_t *destination, uint16_t value);
void net_store_be32(uint8_t *destination, uint32_t value);
uint16_t net_checksum(const uint8_t *data, size_t length);
int net_checksum_valid(const uint8_t *data, size_t length);

int net_parse_ethernet(const uint8_t *frame, size_t frame_length,
                       const uint8_t local_mac[NET_MAC_SIZE],
                       int allow_broadcast,
                       struct net_ethernet_view *view);
int net_build_arp_request(uint8_t *frame, size_t capacity,
                          const uint8_t local_mac[NET_MAC_SIZE],
                          const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE],
                          const uint8_t target_ip[NET_IPV4_ADDRESS_SIZE],
                          size_t *frame_length);
int net_parse_arp_reply(
    const uint8_t *frame, size_t frame_length,
    const uint8_t local_mac[NET_MAC_SIZE],
    const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE],
    const uint8_t sender_ip[NET_IPV4_ADDRESS_SIZE],
    uint8_t sender_mac[NET_MAC_SIZE]);
int net_build_icmp_echo_request(
    uint8_t *frame, size_t capacity,
    const uint8_t local_mac[NET_MAC_SIZE],
    const uint8_t destination_mac[NET_MAC_SIZE],
    const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE],
    const uint8_t destination_ip[NET_IPV4_ADDRESS_SIZE],
    uint16_t ip_identification, uint16_t identifier, uint16_t sequence,
    const uint8_t *payload, size_t payload_length, size_t *frame_length);
int net_validate_icmp_echo_reply(
    const uint8_t *frame, size_t frame_length,
    const uint8_t local_mac[NET_MAC_SIZE],
    const uint8_t source_mac[NET_MAC_SIZE],
    const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE],
    const uint8_t source_ip[NET_IPV4_ADDRESS_SIZE],
    uint16_t identifier, uint16_t sequence,
    const uint8_t *payload, size_t payload_length);
int net_build_udp_datagram(
    uint8_t *frame, size_t capacity,
    const uint8_t local_mac[NET_MAC_SIZE],
    const uint8_t destination_mac[NET_MAC_SIZE],
    const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE],
    const uint8_t destination_ip[NET_IPV4_ADDRESS_SIZE],
    uint16_t ip_identification, uint16_t source_port,
    uint16_t destination_port, const uint8_t *payload,
    size_t payload_length, size_t *frame_length);
/* A null expected source address/MAC or a zero expected port is a wildcard. */
int net_parse_udp_datagram(
    const uint8_t *frame, size_t frame_length,
    const uint8_t destination_mac[NET_MAC_SIZE],
    const uint8_t expected_source_mac[NET_MAC_SIZE],
    const uint8_t destination_ip[NET_IPV4_ADDRESS_SIZE],
    const uint8_t expected_source_ip[NET_IPV4_ADDRESS_SIZE],
    uint16_t expected_destination_port, uint16_t expected_source_port,
    struct net_udp_view *view);

#endif
