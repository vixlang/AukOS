#include <aukos/descriptor.h>
#include <aukos/net.h>
#include <aukos/pipe.h>
#include <aukos/vfs.h>

#include <stddef.h>

static int file_close_count;
static int socket_close_count;
static int tcp_close_count;

int vfs_close(struct vfs_file *file)
{
    if (file) {
        file_close_count++;
    }
    return 0;
}

void net_udp_socket_close(struct udp_socket *socket)
{
    if (socket) {
        socket_close_count++;
    }
}

void net_tcp_socket_close(struct tcp_socket *socket)
{
    if (socket) tcp_close_count++;
}

static int expect(int condition)
{
    return condition ? 0 : 1;
}

int main(void)
{
    struct vfs_file file = {0};
    struct udp_socket *socket = (struct udp_socket *)(void *)&socket_close_count;
    struct tcp_socket *tcp = (struct tcp_socket *)(void *)&tcp_close_count;
    struct descriptor *file_descriptor;
    struct descriptor *socket_descriptor;
    struct descriptor *tcp_descriptor;
    struct descriptor *standard_descriptor;
    struct descriptor *pipe_read_descriptor;
    struct descriptor *pipe_write_descriptor;
    struct pipe_object *pipe;
    struct descriptor *allocated[DESCRIPTOR_MAX_OBJECTS];
    int failures = 0;

    descriptor_init();
    pipe_init();
    file_descriptor = descriptor_create_vfs(&file);
    socket_descriptor = descriptor_create_udp(socket);
    tcp_descriptor = descriptor_create_tcp(tcp);
    standard_descriptor = descriptor_create_standard(1u);
    failures += expect(file_descriptor && socket_descriptor && tcp_descriptor &&
                       standard_descriptor);
    failures += expect(descriptor_vfs_file(file_descriptor) == &file);
    failures += expect(descriptor_udp_socket(file_descriptor) == 0);
    failures += expect(descriptor_udp_socket(socket_descriptor) == socket);
    failures += expect(descriptor_tcp_socket(tcp_descriptor) == tcp);
    failures += expect(descriptor_tcp_socket(socket_descriptor) == 0);
    failures += expect(descriptor_vfs_file(socket_descriptor) == 0);
    failures += expect(descriptor_standard_fd(standard_descriptor) == 1 &&
                       descriptor_standard_fd(file_descriptor) == -1);
    failures += expect(descriptor_live_count() == 4u);

    failures += expect(descriptor_retain(file_descriptor) == 0 &&
                       file_descriptor->references == 2u);
    descriptor_release(file_descriptor);
    failures += expect(file_close_count == 0 &&
                       file_descriptor->references == 1u);
    descriptor_release(file_descriptor);
    failures += expect(file_close_count == 1 &&
                        descriptor_live_count() == 3u);
    descriptor_release(file_descriptor);
    failures += expect(file_close_count == 1);
    descriptor_release(socket_descriptor);
    descriptor_release(tcp_descriptor);
    descriptor_release(standard_descriptor);
    failures += expect(socket_close_count == 1 && tcp_close_count == 1 &&
                       descriptor_live_count() == 0u);

    pipe = pipe_allocate();
    pipe_read_descriptor = descriptor_create_pipe(pipe, 0);
    pipe_write_descriptor = descriptor_create_pipe(pipe, 1);
    failures += expect(pipe && pipe_read_descriptor && pipe_write_descriptor &&
                       pipe_reader_count(pipe) == 1u &&
                       pipe_writer_count(pipe) == 1u);
    failures += expect(descriptor_retain(pipe_read_descriptor) == 0);
    descriptor_release(pipe_read_descriptor);
    failures += expect(pipe_reader_count(pipe) == 1u);
    descriptor_release(pipe_read_descriptor);
    failures += expect(pipe_reader_count(pipe) == 0u && pipe_live_count() == 1u);
    descriptor_release(pipe_write_descriptor);
    failures += expect(pipe_live_count() == 0u);

    for (size_t index = 0; index < DESCRIPTOR_MAX_OBJECTS; index++) {
        allocated[index] = descriptor_create_vfs(&file);
        failures += expect(allocated[index] != 0);
    }
    failures += expect(descriptor_create_vfs(&file) == 0);
    for (size_t index = 0; index < DESCRIPTOR_MAX_OBJECTS; index++) {
        descriptor_release(allocated[index]);
    }
    failures += expect(file_close_count == 1 + DESCRIPTOR_MAX_OBJECTS &&
                       descriptor_live_count() == 0u);
    return failures == 0 ? 0 : 1;
}
