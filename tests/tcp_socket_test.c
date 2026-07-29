#include <aukos/tcp_socket.h>

#include <stddef.h>
#include <stdint.h>

static int expect(int condition) { return condition ? 0 : 1; }

int main(void)
{
    static const uint8_t local_ip[4] = {10, 0, 2, 15};
    static const uint8_t peer_ip[4] = {10, 0, 2, 2};
    static const uint8_t payload[] = "hello";
    struct tcp_socket *allocated[TCP_SOCKET_LIMIT];
    struct tcp_socket *socket;
    struct net_tcp_view view = {0};
    uint8_t output[8];
    int ack = 0;
    int failures = 0;

    tcp_socket_table_init();
    for (size_t i = 0; i < TCP_SOCKET_LIMIT; i++) {
        allocated[i] = tcp_socket_allocate();
        failures += expect(allocated[i] != 0);
    }
    failures += expect(tcp_socket_allocate() == 0);
    for (size_t i = 0; i < TCP_SOCKET_LIMIT; i++) tcp_socket_free(allocated[i]);

    socket = tcp_socket_allocate();
    failures += expect(tcp_socket_begin_connect(socket, local_ip, 49152u,
                                                peer_ip, 80u, 1000u) == 0);
    failures += expect(tcp_socket_find(49152u, peer_ip, 80u) == socket);
    view.flags = NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK;
    view.sequence = 5000u;
    view.acknowledgment = 1001u;
    failures += expect(tcp_socket_handle_segment(socket, &view, &ack) == 0 &&
                       ack && socket->state == TCP_SOCKET_ESTABLISHED &&
                       socket->receive_next == 5001u);

    view.flags = NET_TCP_FLAG_ACK | NET_TCP_FLAG_PSH;
    view.sequence = 5001u;
    view.acknowledgment = 1001u;
    view.payload = payload;
    view.payload_length = sizeof(payload) - 1u;
    failures += expect(tcp_socket_handle_segment(socket, &view, &ack) == 0 && ack);
    failures += expect(tcp_socket_read(socket, output, 3u) == 3u &&
                       output[0] == 'h' && output[2] == 'l');
    failures += expect(tcp_socket_read(socket, output, sizeof(output)) == 2u &&
                       output[0] == 'l' && output[1] == 'o');

    view.flags = NET_TCP_FLAG_ACK | NET_TCP_FLAG_FIN;
    view.sequence = 5006u;
    view.payload = 0;
    view.payload_length = 0u;
    failures += expect(tcp_socket_handle_segment(socket, &view, &ack) == 0 &&
                       ack && tcp_socket_eof(socket));
    tcp_socket_free(socket);
    return failures == 0 ? 0 : 1;
}
