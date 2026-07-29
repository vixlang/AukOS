#include <aukos/net_packets.h>

#include <stddef.h>
#include <stdint.h>

static int expect(int condition) { return condition ? 0 : 1; }

int main(void)
{
    static const uint8_t local_mac[6] = {0x52, 0x54, 0, 0x12, 0x34, 0x56};
    static const uint8_t peer_mac[6] = {0x52, 0x55, 0x0a, 0, 2, 2};
    static const uint8_t local_ip[4] = {10, 0, 2, 15};
    static const uint8_t peer_ip[4] = {10, 0, 2, 2};
    static const uint8_t payload[] = "GET /";
    uint8_t frame[NET_ETHERNET_MAX_FRAME_SIZE];
    struct net_tcp_view view;
    size_t length = 0;
    int failures = 0;

    failures += expect(net_build_tcp_segment(
        frame, sizeof(frame), peer_mac, local_mac, peer_ip, local_ip,
        1u, 80u, 49152u, 100u, 42u,
        NET_TCP_FLAG_ACK | NET_TCP_FLAG_PSH, 8192u,
        payload, sizeof(payload) - 1u, &length) == 0);
    failures += expect(net_parse_tcp_segment(
        frame, length, local_mac, peer_mac, local_ip, peer_ip,
        49152u, 80u, &view) == 0);
    failures += expect(view.sequence == 100u && view.acknowledgment == 42u &&
                       view.flags == (NET_TCP_FLAG_ACK | NET_TCP_FLAG_PSH) &&
                       view.payload_length == sizeof(payload) - 1u &&
                       view.payload[4] == '/');
    frame[NET_ETHERNET_HEADER_SIZE + NET_IPV4_HEADER_SIZE + 16u] ^= 1u;
    failures += expect(net_parse_tcp_segment(
        frame, length, local_mac, peer_mac, local_ip, peer_ip,
        49152u, 80u, &view) != 0);
    failures += expect(net_build_tcp_segment(
        frame, sizeof(frame), peer_mac, local_mac, peer_ip, local_ip,
        1u, 80u, 49152u, 0u, 0u, NET_TCP_FLAG_SYN, 8192u,
        0, 0u, &length) == 0);
    failures += expect(net_parse_tcp_segment(
        frame, length, local_mac, peer_mac, local_ip, peer_ip,
        49152u, 80u, &view) == 0 && view.flags == NET_TCP_FLAG_SYN);
    return failures == 0 ? 0 : 1;
}
