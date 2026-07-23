#include <aukos/udp_socket.h>

#include <stddef.h>
#include <stdint.h>

static int expect(int condition)
{
    return condition ? 0 : 1;
}

int main(void)
{
    static const uint8_t any_ip[NET_IPV4_ADDRESS_SIZE] = {0u, 0u, 0u, 0u};
    static const uint8_t source_ip[NET_IPV4_ADDRESS_SIZE] = {10u, 0u, 2u, 2u};
    static const uint8_t first[] = "first";
    static const uint8_t second[] = "second";
    struct udp_socket *allocated[UDP_SOCKET_LIMIT];
    struct udp_socket *socket;
    struct udp_socket *conflict;
    uint8_t output[16];
    uint8_t received_ip[NET_IPV4_ADDRESS_SIZE];
    uint16_t received_port = 0u;
    size_t copied = 0u;
    int failures = 0;

    udp_socket_table_init();
    for (size_t index = 0; index < UDP_SOCKET_LIMIT; index++) {
        allocated[index] = udp_socket_allocate();
        failures += expect(allocated[index] != 0);
    }
    failures += expect(udp_socket_allocate() == 0);
    for (size_t index = 0; index < UDP_SOCKET_LIMIT; index++) {
        udp_socket_free(allocated[index]);
    }

    socket = udp_socket_allocate();
    conflict = udp_socket_allocate();
    failures += expect(udp_socket_bind_local(socket, any_ip, 49152u) == 0);
    failures += expect(udp_socket_bind_local(socket, any_ip, 49153u) != 0);
    failures += expect(udp_socket_bind_local(conflict, any_ip, 49152u) != 0);
    failures += expect(udp_socket_find_bound(49152u) == socket);

    failures += expect(udp_socket_enqueue(socket, source_ip, 45454u,
                                          first, sizeof(first) - 1u) == 0);
    failures += expect(udp_socket_enqueue(socket, source_ip, 45454u,
                                          second, sizeof(second) - 1u) == 0);
    failures += expect(udp_socket_dequeue(socket, output, 3u, received_ip,
                                          &received_port, &copied) == 0);
    failures += expect(copied == 3u && output[0] == 'f' && output[2] == 'r' &&
                       received_port == 45454u && received_ip[3] == 2u);
    failures += expect(udp_socket_dequeue(socket, output, sizeof(output),
                                          received_ip, &received_port,
                                          &copied) == 0);
    failures += expect(copied == sizeof(second) - 1u && output[0] == 's' &&
                       output[copied - 1u] == 'd');

    for (size_t index = 0; index < UDP_SOCKET_QUEUE_LIMIT; index++) {
        failures += expect(udp_socket_enqueue(socket, source_ip, 45454u,
                                              first, sizeof(first) - 1u) == 0);
    }
    failures += expect(udp_socket_enqueue(socket, source_ip, 45454u,
                                          first, sizeof(first) - 1u) != 0);
    udp_socket_free(socket);
    failures += expect(udp_socket_find_bound(49152u) == 0 &&
                       udp_socket_bind_local(conflict, any_ip, 49152u) == 0);
    udp_socket_free(conflict);
    return failures == 0 ? 0 : 1;
}
