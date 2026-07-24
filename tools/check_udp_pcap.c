#include <aukos/net_packets.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const uint8_t local_mac[NET_MAC_SIZE] =
    {0x52u, 0x54u, 0x00u, 0x12u, 0x34u, 0x56u};
static const uint8_t gateway_mac[NET_MAC_SIZE] =
    {0x52u, 0x55u, 0x0au, 0x00u, 0x02u, 0x02u};
static const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE] = {10u, 0u, 2u, 15u};
static const uint8_t gateway_ip[NET_IPV4_ADDRESS_SIZE] = {10u, 0u, 2u, 2u};
static const uint8_t payload[] = "AukOS UDP echo!";

static uint32_t load_le32(const uint8_t *value)
{
    return value[0] | ((uint32_t)value[1] << 8) |
           ((uint32_t)value[2] << 16) | ((uint32_t)value[3] << 24);
}

static int payload_matches(const struct net_udp_view *view)
{
    return view->payload_length == sizeof(payload) - 1u &&
           memcmp(view->payload, payload, sizeof(payload) - 1u) == 0;
}

int main(int argc, char **argv)
{
    uint8_t global_header[24];
    uint8_t record_header[16];
    uint8_t frame[NET_ETHERNET_MAX_FRAME_SIZE];
    struct net_udp_view view;
    FILE *input;
    int request_found = 0;
    int reply_found = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: %s PCAP\n", argv[0]);
        return 2;
    }
    input = fopen(argv[1], "rb");
    if (!input || fread(global_header, sizeof(global_header), 1u, input) != 1u ||
        load_le32(global_header) != 0xa1b2c3d4u ||
        load_le32(global_header + 20u) != 1u) {
        fprintf(stderr, "invalid Ethernet pcap: %s\n", argv[1]);
        return 1;
    }

    while (fread(record_header, sizeof(record_header), 1u, input) == 1u) {
        uint32_t captured_length = load_le32(record_header + 8u);

        if (captured_length > sizeof(frame)) {
            if (fseek(input, (long)captured_length, SEEK_CUR) != 0) {
                return 1;
            }
            continue;
        }
        if (fread(frame, captured_length, 1u, input) != 1u) {
            fprintf(stderr, "truncated pcap record: %s\n", argv[1]);
            return 1;
        }
        if (net_parse_udp_datagram(
                frame, captured_length, gateway_mac, local_mac, gateway_ip,
                local_ip, 45454u, 49152u, &view) == 0 &&
            payload_matches(&view)) {
            request_found++;
        }
        if (net_parse_udp_datagram(
                frame, captured_length, local_mac, gateway_mac, local_ip,
                gateway_ip, 49152u, 45454u, &view) == 0 &&
            payload_matches(&view)) {
            reply_found++;
        }
    }
    fclose(input);
    if (request_found != 1 || reply_found != 1) {
        fprintf(stderr, "UDP pcap exchange mismatch: request=%d reply=%d\n",
                request_found, reply_found);
        return 1;
    }
    printf("UDP pcap passed: %s (10.0.2.15:49152 <-> 10.0.2.2:45454, 15 bytes)\n",
           argv[1]);
    return 0;
}
