#include <aukos/net_packets.h>

#include <stddef.h>
#include <stdint.h>

static const uint8_t local_mac[NET_MAC_SIZE] =
    {0x52u, 0x54u, 0x00u, 0x12u, 0x34u, 0x56u};
static const uint8_t gateway_mac[NET_MAC_SIZE] =
    {0x52u, 0x55u, 0x0au, 0x00u, 0x02u, 0x02u};
static const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE] = {10u, 0u, 2u, 15u};
static const uint8_t gateway_ip[NET_IPV4_ADDRESS_SIZE] = {10u, 0u, 2u, 2u};

static int expect(int condition)
{
    return condition ? 0 : 1;
}

static void recompute_ipv4_checksum(uint8_t *frame)
{
    uint8_t *ipv4 = frame + NET_ETHERNET_HEADER_SIZE;

    net_store_be16(ipv4 + 10u, 0u);
    net_store_be16(ipv4 + 10u, net_checksum(ipv4, NET_IPV4_HEADER_SIZE));
}

static int build_reply(uint8_t *frame, size_t capacity,
                       const uint8_t *payload, size_t payload_length,
                       size_t *frame_length)
{
    return net_build_udp_datagram(
        frame, capacity, gateway_mac, local_mac, gateway_ip, local_ip,
        0x5544u, 45454u, 49152u, payload, payload_length, frame_length);
}

static int parse_reply(const uint8_t *frame, size_t frame_length,
                       struct net_udp_view *view)
{
    return net_parse_udp_datagram(
        frame, frame_length, local_mac, gateway_mac, local_ip, gateway_ip,
        49152u, 45454u, view);
}

static int test_normal_and_odd_vectors(void)
{
    static const uint8_t even_payload[] = {'e', 'c', 'h', 'o'};
    static const uint8_t odd_payload[] = {'o', 'd', 'd'};
    uint8_t frame[NET_ETHERNET_MAX_FRAME_SIZE];
    struct net_udp_view view;
    size_t frame_length = 0u;
    const uint8_t *ipv4;
    const uint8_t *udp;
    int failures = 0;

    failures += expect(build_reply(frame, sizeof(frame), even_payload,
                                   sizeof(even_payload), &frame_length) == 0);
    ipv4 = frame + NET_ETHERNET_HEADER_SIZE;
    udp = ipv4 + NET_IPV4_HEADER_SIZE;
    failures += expect(frame_length == NET_ETHERNET_MIN_FRAME_SIZE);
    failures += expect(net_load_be16(frame + 12u) == NET_ETHERTYPE_IPV4);
    failures += expect(ipv4[0] == 0x45u && ipv4[9] == 17u);
    failures += expect(net_load_be16(ipv4 + 2u) ==
                       NET_IPV4_HEADER_SIZE + NET_UDP_HEADER_SIZE +
                       sizeof(even_payload));
    failures += expect(net_load_be16(udp) == 45454u &&
                       net_load_be16(udp + 2u) == 49152u);
    failures += expect(net_load_be16(udp + 4u) ==
                       NET_UDP_HEADER_SIZE + sizeof(even_payload));
    failures += expect(net_load_be16(udp + 6u) != 0u);
    failures += expect(parse_reply(frame, frame_length, &view) == 0);
    failures += expect(view.payload_length == sizeof(even_payload) &&
                       view.checksum_present &&
                       view.payload[0] == 'e' && view.payload[3] == 'o');
    frame[frame_length - 1u] = 0xa5u;
    failures += expect(parse_reply(frame, frame_length, &view) == 0);

    failures += expect(build_reply(frame, sizeof(frame), odd_payload,
                                   sizeof(odd_payload), &frame_length) == 0);
    failures += expect(parse_reply(frame, frame_length, &view) == 0 &&
                       view.payload_length == sizeof(odd_payload) &&
                       view.payload[2] == 'd');
    return failures;
}

static int test_payload_boundaries(void)
{
    uint8_t frame[NET_ETHERNET_MAX_FRAME_SIZE];
    uint8_t payload[NET_UDP_MAX_PAYLOAD + 1u];
    struct net_udp_view view;
    size_t frame_length = 0u;
    int failures = 0;

    for (size_t index = 0; index < sizeof(payload); index++) {
        payload[index] = (uint8_t)index;
    }
    failures += expect(build_reply(frame, sizeof(frame), 0, 0,
                                   &frame_length) == 0);
    failures += expect(frame_length == NET_ETHERNET_MIN_FRAME_SIZE &&
                       parse_reply(frame, frame_length, &view) == 0 &&
                       view.payload_length == 0u);
    failures += expect(build_reply(frame, sizeof(frame), payload,
                                   NET_UDP_MAX_PAYLOAD,
                                   &frame_length) == 0);
    failures += expect(frame_length == NET_ETHERNET_MAX_FRAME_SIZE &&
                       parse_reply(frame, frame_length, &view) == 0 &&
                       view.payload_length == NET_UDP_MAX_PAYLOAD &&
                       view.payload[NET_UDP_MAX_PAYLOAD - 1u] ==
                           payload[NET_UDP_MAX_PAYLOAD - 1u]);
    failures += expect(build_reply(frame, sizeof(frame), payload,
                                   NET_UDP_MAX_PAYLOAD + 1u,
                                   &frame_length) != 0);
    failures += expect(build_reply(frame, NET_ETHERNET_MAX_FRAME_SIZE - 1u,
                                   payload, NET_UDP_MAX_PAYLOAD,
                                   &frame_length) != 0);
    return failures;
}

static int test_zero_checksum_policy_and_encoding(void)
{
    uint8_t frame[NET_ETHERNET_MIN_FRAME_SIZE];
    uint8_t payload[2];
    uint8_t *udp = frame + NET_ETHERNET_HEADER_SIZE + NET_IPV4_HEADER_SIZE;
    struct net_udp_view view;
    size_t frame_length = 0u;
    int found_ffff = 0;
    int failures = 0;

    for (uint32_t value = 0u; value <= 0xffffu; value++) {
        payload[0] = (uint8_t)(value >> 8);
        payload[1] = (uint8_t)value;
        if (build_reply(frame, sizeof(frame), payload, sizeof(payload),
                        &frame_length) == 0 &&
            net_load_be16(udp + 6u) == 0xffffu) {
            found_ffff = 1;
            break;
        }
    }
    failures += expect(found_ffff && parse_reply(frame, frame_length, &view) == 0);

    net_store_be16(udp + 6u, 0u);
    failures += expect(parse_reply(frame, frame_length, &view) == 0 &&
                       !view.checksum_present);
    return failures;
}

static int test_rejections(void)
{
    static const uint8_t payload[] = "udp-vector";
    uint8_t frame[NET_ETHERNET_MAX_FRAME_SIZE];
    uint8_t *ipv4 = frame + NET_ETHERNET_HEADER_SIZE;
    uint8_t *udp = ipv4 + NET_IPV4_HEADER_SIZE;
    struct net_udp_view view;
    size_t frame_length = 0u;
    int failures = 0;

    build_reply(frame, sizeof(frame), payload, sizeof(payload) - 1u,
                &frame_length);
    failures += expect(parse_reply(frame, 13u, &view) != 0);
    failures += expect(parse_reply(frame, NET_ETHERNET_HEADER_SIZE + 19u,
                                   &view) != 0);
    failures += expect(parse_reply(frame, frame_length -
                                   (NET_ETHERNET_MIN_FRAME_SIZE -
                                    (NET_ETHERNET_HEADER_SIZE +
                                     net_load_be16(ipv4 + 2u))) - 1u,
                                   &view) != 0);

#define REBUILD() build_reply(frame, sizeof(frame), payload, \
                              sizeof(payload) - 1u, &frame_length)
    REBUILD();
    frame[0] ^= 1u;
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    frame[6] ^= 1u;
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    ipv4[0] = 0x55u;
    recompute_ipv4_checksum(frame);
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    ipv4[0] = 0x46u;
    recompute_ipv4_checksum(frame);
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    net_store_be16(ipv4 + 2u, NET_IPV4_HEADER_SIZE + NET_UDP_HEADER_SIZE - 1u);
    recompute_ipv4_checksum(frame);
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    net_store_be16(ipv4 + 2u, (uint16_t)(frame_length + 1u));
    recompute_ipv4_checksum(frame);
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    ipv4[10] ^= 1u;
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    net_store_be16(ipv4 + 6u, 1u);
    recompute_ipv4_checksum(frame);
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    ipv4[9] = 1u;
    recompute_ipv4_checksum(frame);
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    ipv4[12] ^= 1u;
    recompute_ipv4_checksum(frame);
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    ipv4[16] ^= 1u;
    recompute_ipv4_checksum(frame);
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    net_store_be16(udp + 4u, NET_UDP_HEADER_SIZE - 1u);
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    net_store_be16(udp + 4u, net_load_be16(udp + 4u) - 1u);
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    net_store_be16(udp, 0u);
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    net_store_be16(udp + 2u, 0u);
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    udp[6] ^= 1u;
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    udp[NET_UDP_HEADER_SIZE] ^= 1u;
    failures += expect(parse_reply(frame, frame_length, &view) != 0);
    REBUILD();
    failures += expect(net_parse_udp_datagram(
        frame, frame_length, local_mac, gateway_mac, local_ip, gateway_ip,
        49153u, 45454u, &view) != 0);
    failures += expect(net_parse_udp_datagram(
        frame, frame_length, local_mac, gateway_mac, local_ip, gateway_ip,
        49152u, 45455u, &view) != 0);
#undef REBUILD
    return failures;
}

int main(void)
{
    int failures = 0;

    failures += test_normal_and_odd_vectors();
    failures += test_payload_boundaries();
    failures += test_zero_checksum_policy_and_encoding();
    failures += test_rejections();
    return failures == 0 ? 0 : 1;
}
