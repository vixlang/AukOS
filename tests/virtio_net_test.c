#include <aukos/net_packets.h>
#include <aukos/virtio_net.h>

#include <stddef.h>
#include <stdint.h>

static const uint8_t local_mac[NET_MAC_SIZE] =
    {0x52u, 0x54u, 0x00u, 0x12u, 0x34u, 0x56u};
static const uint8_t gateway_mac[NET_MAC_SIZE] =
    {0x52u, 0x55u, 0x0au, 0x00u, 0x02u, 0x02u};
static const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE] = {10u, 0u, 2u, 15u};
static const uint8_t gateway_ip[NET_IPV4_ADDRESS_SIZE] = {10u, 0u, 2u, 2u};
static const uint8_t echo_payload[] = "AukOS ICMP echo";

static int expect(int condition)
{
    return condition ? 0 : 1;
}

static void copy_bytes(uint8_t *destination, const uint8_t *source, size_t size)
{
    for (size_t index = 0; index < size; index++) {
        destination[index] = source[index];
    }
}

static void fill_bytes(uint8_t *destination, uint8_t value, size_t size)
{
    for (size_t index = 0; index < size; index++) {
        destination[index] = value;
    }
}

static void make_arp_reply(uint8_t frame[NET_ETHERNET_MIN_FRAME_SIZE])
{
    uint8_t *arp = frame + NET_ETHERNET_HEADER_SIZE;

    fill_bytes(frame, 0u, NET_ETHERNET_MIN_FRAME_SIZE);
    copy_bytes(frame, local_mac, NET_MAC_SIZE);
    copy_bytes(frame + 6u, gateway_mac, NET_MAC_SIZE);
    net_store_be16(frame + 12u, NET_ETHERTYPE_ARP);
    net_store_be16(arp, 1u);
    net_store_be16(arp + 2u, NET_ETHERTYPE_IPV4);
    arp[4] = NET_MAC_SIZE;
    arp[5] = NET_IPV4_ADDRESS_SIZE;
    net_store_be16(arp + 6u, 2u);
    copy_bytes(arp + 8u, gateway_mac, NET_MAC_SIZE);
    copy_bytes(arp + 14u, gateway_ip, NET_IPV4_ADDRESS_SIZE);
    copy_bytes(arp + 18u, local_mac, NET_MAC_SIZE);
    copy_bytes(arp + 24u, local_ip, NET_IPV4_ADDRESS_SIZE);
}

static void recompute_ipv4_checksum(uint8_t *frame)
{
    uint8_t *ipv4 = frame + NET_ETHERNET_HEADER_SIZE;

    net_store_be16(ipv4 + 10u, 0u);
    net_store_be16(ipv4 + 10u, net_checksum(ipv4, NET_IPV4_HEADER_SIZE));
}

static void recompute_icmp_checksum(uint8_t *frame)
{
    uint8_t *ipv4 = frame + NET_ETHERNET_HEADER_SIZE;
    uint8_t *icmp = ipv4 + NET_IPV4_HEADER_SIZE;
    size_t icmp_length = net_load_be16(ipv4 + 2u) - NET_IPV4_HEADER_SIZE;

    net_store_be16(icmp + 2u, 0u);
    net_store_be16(icmp + 2u, net_checksum(icmp, icmp_length));
}

static int build_echo_reply(uint8_t *frame, size_t capacity,
                            size_t *frame_length)
{
    uint8_t *ipv4;
    uint8_t *icmp;

    if (net_build_icmp_echo_request(
            frame, capacity, local_mac, gateway_mac, local_ip, gateway_ip,
            0x4f53u, 0xa905u, 1u, echo_payload,
            sizeof(echo_payload) - 1u, frame_length) != 0) {
        return -1;
    }
    copy_bytes(frame, local_mac, NET_MAC_SIZE);
    copy_bytes(frame + 6u, gateway_mac, NET_MAC_SIZE);
    ipv4 = frame + NET_ETHERNET_HEADER_SIZE;
    copy_bytes(ipv4 + 12u, gateway_ip, NET_IPV4_ADDRESS_SIZE);
    copy_bytes(ipv4 + 16u, local_ip, NET_IPV4_ADDRESS_SIZE);
    recompute_ipv4_checksum(frame);
    icmp = ipv4 + NET_IPV4_HEADER_SIZE;
    icmp[0] = 0u;
    recompute_icmp_checksum(frame);
    return 0;
}

static int validate_echo(const uint8_t *frame, size_t frame_length)
{
    return net_validate_icmp_echo_reply(
        frame, frame_length, local_mac, gateway_mac, local_ip, gateway_ip,
        0xa905u, 1u, echo_payload, sizeof(echo_payload) - 1u);
}

static int test_byte_order_and_checksums(void)
{
    static const uint8_t ipv4_vector[NET_IPV4_HEADER_SIZE] = {
        0x45u, 0x00u, 0x00u, 0x73u, 0x00u, 0x00u, 0x40u, 0x00u,
        0x40u, 0x11u, 0x00u, 0x00u, 0xc0u, 0xa8u, 0x00u, 0x01u,
        0xc0u, 0xa8u, 0x00u, 0xc7u
    };
    static const uint8_t icmp_vector[] = {
        0x08u, 0x00u, 0x00u, 0x00u, 0x12u, 0x34u, 0x00u, 0x01u,
        0x61u, 0x62u, 0x63u
    };
    static const uint8_t odd_vector[] = {0x01u, 0x02u, 0x03u};
    uint8_t storage[4];
    uint8_t valid_icmp[sizeof(icmp_vector)];
    int failures = 0;

    net_store_be16(storage, 0x1234u);
    failures += expect(storage[0] == 0x12u && storage[1] == 0x34u);
    failures += expect(net_load_be16(storage) == 0x1234u);
    net_store_be32(storage, 0x12345678u);
    failures += expect(net_load_be32(storage) == 0x12345678u);
    failures += expect(net_checksum(ipv4_vector, sizeof(ipv4_vector)) ==
                       0xb861u);
    failures += expect(net_checksum(icmp_vector, sizeof(icmp_vector)) ==
                       0x2168u);
    failures += expect(net_checksum(odd_vector, sizeof(odd_vector)) ==
                       0xfbfdu);
    copy_bytes(valid_icmp, icmp_vector, sizeof(valid_icmp));
    net_store_be16(valid_icmp + 2u, 0x2168u);
    failures += expect(net_checksum_valid(valid_icmp, sizeof(valid_icmp)));
    return failures;
}

static int test_ethernet_and_arp(void)
{
    uint8_t frame[NET_ETHERNET_MIN_FRAME_SIZE];
    uint8_t learned_mac[NET_MAC_SIZE];
    struct net_ethernet_view view;
    size_t frame_length = 0u;
    int failures = 0;

    failures += expect(sizeof(struct virtio_net_hdr) == 10u);
    failures += expect(net_build_arp_request(
        frame, sizeof(frame), local_mac, local_ip, gateway_ip,
        &frame_length) == 0);
    failures += expect(frame_length == NET_ETHERNET_MIN_FRAME_SIZE);
    failures += expect(frame[0] == 0xffu && frame[5] == 0xffu);
    failures += expect(frame[6] == local_mac[0] && frame[11] == local_mac[5]);
    failures += expect(net_load_be16(frame + 12u) == NET_ETHERTYPE_ARP);
    failures += expect(net_load_be16(frame + 14u) == 1u);
    failures += expect(net_load_be16(frame + 16u) == NET_ETHERTYPE_IPV4);
    failures += expect(frame[18] == 6u && frame[19] == 4u);
    failures += expect(net_load_be16(frame + 20u) == 1u);
    failures += expect(frame[28] == 10u && frame[31] == 15u);
    failures += expect(frame[38] == 10u && frame[41] == 2u);
    failures += expect(net_build_arp_request(
        frame, NET_ETHERNET_MIN_FRAME_SIZE - 1u, local_mac, local_ip,
        gateway_ip, &frame_length) != 0);
    failures += expect(net_parse_ethernet(frame, 13u, local_mac, 1, &view) != 0);
    failures += expect(net_parse_ethernet(frame, frame_length, local_mac, 1,
                                          &view) == 0);
    failures += expect(view.destination_is_broadcast &&
                       view.ether_type == NET_ETHERTYPE_ARP);
    failures += expect(net_parse_ethernet(frame, frame_length, local_mac, 0,
                                          &view) != 0);
    net_store_be16(frame + 12u, 0x88b5u);
    failures += expect(net_parse_ethernet(frame, frame_length, local_mac, 1,
                                          &view) == 0 &&
                       view.ether_type == 0x88b5u);

    make_arp_reply(frame);
    failures += expect(net_parse_arp_reply(
        frame, sizeof(frame), local_mac, local_ip, gateway_ip,
        learned_mac) == 0);
    failures += expect(learned_mac[0] == gateway_mac[0] &&
                       learned_mac[5] == gateway_mac[5]);
    failures += expect(net_parse_arp_reply(
        frame, NET_ARP_FRAME_SIZE - 1u, local_mac, local_ip, gateway_ip,
        learned_mac) != 0);
    make_arp_reply(frame);
    frame[0] ^= 1u;
    failures += expect(net_parse_arp_reply(
        frame, sizeof(frame), local_mac, local_ip, gateway_ip,
        learned_mac) != 0);
    make_arp_reply(frame);
    frame[14] = 2u;
    failures += expect(net_parse_arp_reply(
        frame, sizeof(frame), local_mac, local_ip, gateway_ip,
        learned_mac) != 0);
    make_arp_reply(frame);
    frame[16] ^= 1u;
    failures += expect(net_parse_arp_reply(
        frame, sizeof(frame), local_mac, local_ip, gateway_ip,
        learned_mac) != 0);
    make_arp_reply(frame);
    frame[18] = 5u;
    failures += expect(net_parse_arp_reply(
        frame, sizeof(frame), local_mac, local_ip, gateway_ip,
        learned_mac) != 0);
    make_arp_reply(frame);
    frame[19] = 6u;
    failures += expect(net_parse_arp_reply(
        frame, sizeof(frame), local_mac, local_ip, gateway_ip,
        learned_mac) != 0);
    make_arp_reply(frame);
    frame[21] = 1u;
    failures += expect(net_parse_arp_reply(
        frame, sizeof(frame), local_mac, local_ip, gateway_ip,
        learned_mac) != 0);
    make_arp_reply(frame);
    frame[31] = 3u;
    failures += expect(net_parse_arp_reply(
        frame, sizeof(frame), local_mac, local_ip, gateway_ip,
        learned_mac) != 0);
    make_arp_reply(frame);
    frame[38] ^= 1u;
    failures += expect(net_parse_arp_reply(
        frame, sizeof(frame), local_mac, local_ip, gateway_ip,
        learned_mac) != 0);
    make_arp_reply(frame);
    frame[32] ^= 1u;
    failures += expect(net_parse_arp_reply(
        frame, sizeof(frame), local_mac, local_ip, gateway_ip,
        learned_mac) != 0);
    make_arp_reply(frame);
    copy_bytes(frame + 6u, local_mac, NET_MAC_SIZE);
    copy_bytes(frame + 22u, local_mac, NET_MAC_SIZE);
    failures += expect(net_parse_arp_reply(
        frame, sizeof(frame), local_mac, local_ip, gateway_ip,
        learned_mac) != 0);
    make_arp_reply(frame);
    frame[22] ^= 1u;
    failures += expect(net_parse_arp_reply(
        frame, sizeof(frame), local_mac, local_ip, gateway_ip,
        learned_mac) != 0);
    return failures;
}

static int test_echo_builder(void)
{
    uint8_t frame[NET_ETHERNET_MAX_FRAME_SIZE];
    const uint8_t *ipv4;
    const uint8_t *icmp;
    size_t frame_length = 0u;
    size_t ipv4_length;
    size_t icmp_length;
    int failures = 0;

    failures += expect(net_build_icmp_echo_request(
        frame, sizeof(frame), local_mac, gateway_mac, local_ip, gateway_ip,
        0x4f53u, 0xa905u, 1u, echo_payload, sizeof(echo_payload) - 1u,
        &frame_length) == 0);
    ipv4 = frame + NET_ETHERNET_HEADER_SIZE;
    icmp = ipv4 + NET_IPV4_HEADER_SIZE;
    ipv4_length = NET_IPV4_HEADER_SIZE + NET_ICMP_ECHO_HEADER_SIZE +
                  sizeof(echo_payload) - 1u;
    icmp_length = ipv4_length - NET_IPV4_HEADER_SIZE;
    failures += expect(frame_length == NET_ETHERNET_MIN_FRAME_SIZE);
    failures += expect(net_load_be16(frame + 12u) == NET_ETHERTYPE_IPV4);
    failures += expect(ipv4[0] == 0x45u && ipv4[1] == 0u);
    failures += expect(net_load_be16(ipv4 + 2u) == ipv4_length);
    failures += expect(net_load_be16(ipv4 + 4u) == 0x4f53u &&
                       net_load_be16(ipv4 + 6u) == 0x4000u);
    failures += expect(ipv4[8] == 64u && ipv4[9] == 1u);
    failures += expect(ipv4[12] == 10u && ipv4[15] == 15u &&
                       ipv4[16] == 10u && ipv4[19] == 2u);
    failures += expect(net_load_be16(ipv4 + 10u) == 0xd36eu);
    failures += expect(net_checksum_valid(ipv4, NET_IPV4_HEADER_SIZE));
    failures += expect(icmp[0] == 8u && icmp[1] == 0u);
    failures += expect(net_load_be16(icmp + 4u) == 0xa905u &&
                       net_load_be16(icmp + 6u) == 1u);
    failures += expect(net_load_be16(icmp + 2u) == 0xc5b2u);
    failures += expect(net_checksum_valid(icmp, icmp_length));
    failures += expect(frame[NET_ETHERNET_HEADER_SIZE + ipv4_length] == 0u &&
                       frame[NET_ETHERNET_MIN_FRAME_SIZE - 1u] == 0u);
    frame[NET_ETHERNET_MIN_FRAME_SIZE - 1u] = 0x5au;
    failures += expect(net_checksum_valid(ipv4, NET_IPV4_HEADER_SIZE));
    failures += expect(net_checksum_valid(icmp, icmp_length));
    failures += expect(net_build_icmp_echo_request(
        frame, NET_ETHERNET_MIN_FRAME_SIZE - 1u, local_mac, gateway_mac,
        local_ip, gateway_ip, 0x4f53u, 0xa905u, 1u, echo_payload,
        sizeof(echo_payload) - 1u, &frame_length) != 0);
    return failures;
}

static int test_echo_reply_rejections(void)
{
    uint8_t frame[NET_ETHERNET_MAX_FRAME_SIZE];
    uint8_t *ipv4 = frame + NET_ETHERNET_HEADER_SIZE;
    uint8_t *icmp = ipv4 + NET_IPV4_HEADER_SIZE;
    size_t frame_length = 0u;
    int failures = 0;

    failures += expect(build_echo_reply(frame, sizeof(frame), &frame_length) == 0);
    failures += expect(validate_echo(frame, frame_length) == 0);
    frame[frame_length - 1u] = 0xa5u;
    failures += expect(validate_echo(frame, frame_length) == 0);
    failures += expect(validate_echo(frame, 13u) != 0);
    failures += expect(validate_echo(frame, NET_ETHERNET_HEADER_SIZE + 19u) != 0);
    failures += expect(validate_echo(frame, NET_ETHERNET_HEADER_SIZE +
                                            NET_IPV4_HEADER_SIZE + 7u) != 0);

    build_echo_reply(frame, sizeof(frame), &frame_length);
    frame[0] ^= 1u;
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    frame[6] ^= 1u;
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    ipv4[0] = 0x55u;
    recompute_ipv4_checksum(frame);
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    ipv4[0] = 0x46u;
    recompute_ipv4_checksum(frame);
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    net_store_be16(ipv4 + 2u, 27u);
    recompute_ipv4_checksum(frame);
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    net_store_be16(ipv4 + 2u, (uint16_t)frame_length);
    recompute_ipv4_checksum(frame);
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    ipv4[10] ^= 1u;
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    net_store_be16(ipv4 + 6u, 1u);
    recompute_ipv4_checksum(frame);
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    net_store_be16(ipv4 + 6u, 0x8000u);
    recompute_ipv4_checksum(frame);
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    ipv4[9] = 17u;
    recompute_ipv4_checksum(frame);
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    ipv4[12] ^= 1u;
    recompute_ipv4_checksum(frame);
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    ipv4[19] ^= 1u;
    recompute_ipv4_checksum(frame);
    failures += expect(validate_echo(frame, frame_length) != 0);

    build_echo_reply(frame, sizeof(frame), &frame_length);
    icmp[0] = 8u;
    recompute_icmp_checksum(frame);
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    icmp[1] = 1u;
    recompute_icmp_checksum(frame);
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    icmp[2] ^= 1u;
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    icmp[5] ^= 1u;
    recompute_icmp_checksum(frame);
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    icmp[7] ^= 1u;
    recompute_icmp_checksum(frame);
    failures += expect(validate_echo(frame, frame_length) != 0);
    build_echo_reply(frame, sizeof(frame), &frame_length);
    icmp[NET_ICMP_ECHO_HEADER_SIZE] ^= 1u;
    recompute_icmp_checksum(frame);
    failures += expect(validate_echo(frame, frame_length) != 0);
    return failures;
}

int main(void)
{
    int failures = 0;

    failures += test_byte_order_and_checksums();
    failures += test_ethernet_and_arp();
    failures += test_echo_builder();
    failures += test_echo_reply_rejections();
    return failures == 0 ? 0 : 1;
}
