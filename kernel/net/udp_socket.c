#include "include/aukos/udp_socket.h"

static struct udp_socket sockets[UDP_SOCKET_LIMIT];

static void bytes_copy(uint8_t *destination, const uint8_t *source, size_t size)
{
    for (size_t index = 0; index < size; index++) {
        destination[index] = source[index];
    }
}

static void socket_reset(struct udp_socket *socket)
{
    socket->allocated = 0;
    socket->bound = 0;
    for (size_t index = 0; index < NET_IPV4_ADDRESS_SIZE; index++) {
        socket->local_ip[index] = 0u;
    }
    socket->local_port = 0u;
    socket->queue_head = 0u;
    socket->queue_count = 0u;
}

void udp_socket_table_init(void)
{
    for (size_t index = 0; index < UDP_SOCKET_LIMIT; index++) {
        socket_reset(&sockets[index]);
    }
}

struct udp_socket *udp_socket_allocate(void)
{
    for (size_t index = 0; index < UDP_SOCKET_LIMIT; index++) {
        if (!sockets[index].allocated) {
            socket_reset(&sockets[index]);
            sockets[index].allocated = 1;
            return &sockets[index];
        }
    }
    return 0;
}

void udp_socket_free(struct udp_socket *socket)
{
    if (socket) {
        socket_reset(socket);
    }
}

int udp_socket_bind_local(struct udp_socket *socket,
                          const uint8_t local_ip[NET_IPV4_ADDRESS_SIZE],
                          uint16_t local_port)
{
    if (!socket || !socket->allocated || socket->bound || !local_ip ||
        local_port == 0u || udp_socket_find_bound(local_port)) {
        return -1;
    }
    bytes_copy(socket->local_ip, local_ip, NET_IPV4_ADDRESS_SIZE);
    socket->local_port = local_port;
    socket->bound = 1;
    return 0;
}

struct udp_socket *udp_socket_find_bound(uint16_t local_port)
{
    for (size_t index = 0; index < UDP_SOCKET_LIMIT; index++) {
        if (sockets[index].allocated && sockets[index].bound &&
            sockets[index].local_port == local_port) {
            return &sockets[index];
        }
    }
    return 0;
}

int udp_socket_enqueue(struct udp_socket *socket,
                       const uint8_t source_ip[NET_IPV4_ADDRESS_SIZE],
                       uint16_t source_port, const uint8_t *payload,
                       size_t payload_length)
{
    size_t slot;
    struct udp_datagram *datagram;

    if (!socket || !socket->allocated || !socket->bound || !source_ip ||
        source_port == 0u || (!payload && payload_length != 0u) ||
        payload_length > NET_UDP_MAX_PAYLOAD ||
        socket->queue_count >= UDP_SOCKET_QUEUE_LIMIT) {
        return -1;
    }
    slot = (socket->queue_head + socket->queue_count) % UDP_SOCKET_QUEUE_LIMIT;
    datagram = &socket->queue[slot];
    bytes_copy(datagram->source_ip, source_ip, NET_IPV4_ADDRESS_SIZE);
    datagram->source_port = source_port;
    datagram->length = payload_length;
    bytes_copy(datagram->payload, payload, payload_length);
    socket->queue_count++;
    return 0;
}

int udp_socket_dequeue(struct udp_socket *socket, uint8_t *buffer,
                       size_t capacity,
                       uint8_t source_ip[NET_IPV4_ADDRESS_SIZE],
                       uint16_t *source_port, size_t *copied_length)
{
    struct udp_datagram *datagram;
    size_t copy_length;

    if (!socket || !socket->allocated || !buffer || !source_ip ||
        !source_port || !copied_length || socket->queue_count == 0u) {
        return -1;
    }
    datagram = &socket->queue[socket->queue_head];
    copy_length = datagram->length < capacity ? datagram->length : capacity;
    bytes_copy(buffer, datagram->payload, copy_length);
    bytes_copy(source_ip, datagram->source_ip, NET_IPV4_ADDRESS_SIZE);
    *source_port = datagram->source_port;
    *copied_length = copy_length;
    socket->queue_head = (socket->queue_head + 1u) % UDP_SOCKET_QUEUE_LIMIT;
    socket->queue_count--;
    return 0;
}
