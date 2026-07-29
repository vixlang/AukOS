#include "include/aukos/tcp_socket.h"

static struct tcp_socket sockets[TCP_SOCKET_LIMIT];

static void bytes_copy(uint8_t *destination, const uint8_t *source, size_t size)
{
    for (size_t index = 0; index < size; index++) destination[index] = source[index];
}

static int bytes_equal(const uint8_t *left, const uint8_t *right, size_t size)
{
    for (size_t index = 0; index < size; index++) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

static void reset(struct tcp_socket *socket)
{
    socket->allocated = 0;
    socket->state = TCP_SOCKET_CLOSED;
    socket->local_port = 0u;
    socket->remote_port = 0u;
    socket->send_unacknowledged = 0u;
    socket->send_next = 0u;
    socket->receive_next = 0u;
    socket->rx_head = 0u;
    socket->rx_count = 0u;
}

void tcp_socket_table_init(void)
{
    for (size_t index = 0; index < TCP_SOCKET_LIMIT; index++) reset(&sockets[index]);
}

struct tcp_socket *tcp_socket_allocate(void)
{
    for (size_t index = 0; index < TCP_SOCKET_LIMIT; index++) {
        if (!sockets[index].allocated) {
            reset(&sockets[index]);
            sockets[index].allocated = 1;
            return &sockets[index];
        }
    }
    return 0;
}

void tcp_socket_free(struct tcp_socket *socket)
{
    if (socket) reset(socket);
}

struct tcp_socket *tcp_socket_find(uint16_t local_port,
                                   const uint8_t remote_ip[4],
                                   uint16_t remote_port)
{
    for (size_t index = 0; index < TCP_SOCKET_LIMIT; index++) {
        if (sockets[index].allocated && sockets[index].local_port == local_port &&
            sockets[index].remote_port == remote_port &&
            bytes_equal(sockets[index].remote_ip, remote_ip, 4u)) return &sockets[index];
    }
    return 0;
}

int tcp_socket_begin_connect(struct tcp_socket *socket,
                             const uint8_t local_ip[4], uint16_t local_port,
                             const uint8_t remote_ip[4], uint16_t remote_port,
                             uint32_t initial_sequence)
{
    if (!socket || !socket->allocated || socket->state != TCP_SOCKET_CLOSED ||
        !local_ip || !remote_ip || local_port == 0u || remote_port == 0u ||
        tcp_socket_find(local_port, remote_ip, remote_port)) return -1;
    bytes_copy(socket->local_ip, local_ip, 4u);
    bytes_copy(socket->remote_ip, remote_ip, 4u);
    socket->local_port = local_port;
    socket->remote_port = remote_port;
    socket->send_unacknowledged = initial_sequence;
    socket->send_next = initial_sequence + 1u;
    socket->state = TCP_SOCKET_SYN_SENT;
    return 0;
}

static int enqueue(struct tcp_socket *socket, const uint8_t *data, size_t length)
{
    if (length > TCP_SOCKET_RX_CAPACITY - socket->rx_count) return -1;
    for (size_t index = 0; index < length; index++) {
        size_t slot = (socket->rx_head + socket->rx_count) % TCP_SOCKET_RX_CAPACITY;
        socket->rx[slot] = data[index];
        socket->rx_count++;
    }
    return 0;
}

int tcp_socket_handle_segment(struct tcp_socket *socket,
                              const struct net_tcp_view *view,
                              int *ack_required)
{
    uint32_t next;

    if (!socket || !view || !ack_required || !socket->allocated) return -1;
    *ack_required = 0;
    if (view->flags & NET_TCP_FLAG_RST) {
        socket->state = TCP_SOCKET_RESET;
        return -1;
    }
    if (socket->state == TCP_SOCKET_SYN_SENT) {
        if ((view->flags & (NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK)) !=
                (NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK) ||
            view->acknowledgment != socket->send_next) return -1;
        socket->send_unacknowledged = view->acknowledgment;
        socket->receive_next = view->sequence + 1u;
        socket->state = TCP_SOCKET_ESTABLISHED;
        *ack_required = 1;
        return 0;
    }
    if (socket->state != TCP_SOCKET_ESTABLISHED &&
        socket->state != TCP_SOCKET_CLOSE_WAIT) return -1;
    if ((view->flags & NET_TCP_FLAG_ACK) &&
        view->acknowledgment >= socket->send_unacknowledged &&
        view->acknowledgment <= socket->send_next) {
        socket->send_unacknowledged = view->acknowledgment;
    }
    if (view->payload_length == 0u && !(view->flags & NET_TCP_FLAG_FIN)) return 0;
    if (view->sequence != socket->receive_next) {
        *ack_required = 1;
        return 0;
    }
    if (enqueue(socket, view->payload, view->payload_length) != 0) {
        *ack_required = 1;
        return -1;
    }
    next = socket->receive_next + (uint32_t)view->payload_length;
    if (view->flags & NET_TCP_FLAG_FIN) {
        next++;
        socket->state = TCP_SOCKET_CLOSE_WAIT;
    }
    socket->receive_next = next;
    *ack_required = 1;
    return 0;
}

size_t tcp_socket_read(struct tcp_socket *socket, uint8_t *buffer,
                       size_t capacity)
{
    size_t count;
    if (!socket || !buffer) return 0u;
    count = socket->rx_count < capacity ? socket->rx_count : capacity;
    for (size_t index = 0; index < count; index++) {
        buffer[index] = socket->rx[socket->rx_head];
        socket->rx_head = (socket->rx_head + 1u) % TCP_SOCKET_RX_CAPACITY;
        socket->rx_count--;
    }
    return count;
}

int tcp_socket_eof(const struct tcp_socket *socket)
{
    return socket && socket->state == TCP_SOCKET_CLOSE_WAIT && socket->rx_count == 0u;
}
