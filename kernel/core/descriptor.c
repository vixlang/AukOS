#include "include/aukos/descriptor.h"

#include "include/aukos/net.h"
#include "include/aukos/pipe.h"
#include "include/aukos/vfs.h"

#include <limits.h>

static struct descriptor descriptors[DESCRIPTOR_MAX_OBJECTS];

void descriptor_init(void)
{
    for (size_t index = 0; index < DESCRIPTOR_MAX_OBJECTS; index++) {
        descriptors[index].type = DESCRIPTOR_NONE;
        descriptors[index].references = 0u;
        descriptors[index].object.file = 0;
    }
}

static struct descriptor *descriptor_allocate(enum descriptor_type type)
{
    for (size_t index = 0; index < DESCRIPTOR_MAX_OBJECTS; index++) {
        if (descriptors[index].type == DESCRIPTOR_NONE) {
            descriptors[index].type = type;
            descriptors[index].references = 1u;
            return &descriptors[index];
        }
    }
    return 0;
}

struct descriptor *descriptor_create_vfs(struct vfs_file *file)
{
    struct descriptor *descriptor;

    if (!file) {
        return 0;
    }
    descriptor = descriptor_allocate(DESCRIPTOR_VFS_FILE);
    if (descriptor) {
        descriptor->object.file = file;
    }
    return descriptor;
}

struct descriptor *descriptor_create_udp(struct udp_socket *socket)
{
    struct descriptor *descriptor;

    if (!socket) {
        return 0;
    }
    descriptor = descriptor_allocate(DESCRIPTOR_UDP_SOCKET);
    if (descriptor) {
        descriptor->object.udp_socket = socket;
    }
    return descriptor;
}

struct descriptor *descriptor_create_tcp(struct tcp_socket *socket)
{
    struct descriptor *descriptor;

    if (!socket) return 0;
    descriptor = descriptor_allocate(DESCRIPTOR_TCP_SOCKET);
    if (descriptor) descriptor->object.tcp_socket = socket;
    return descriptor;
}

struct descriptor *descriptor_create_standard(uint32_t standard_fd)
{
    struct descriptor *descriptor;

    if (standard_fd > 2u) {
        return 0;
    }
    descriptor = descriptor_allocate(DESCRIPTOR_STANDARD);
    if (descriptor) {
        descriptor->object.standard_fd = standard_fd;
    }
    return descriptor;
}

struct descriptor *descriptor_create_pipe(struct pipe_object *pipe,
                                          int write_end)
{
    struct descriptor *descriptor;
    enum pipe_direction direction = write_end ? PIPE_WRITE_END : PIPE_READ_END;

    if (!pipe) {
        return 0;
    }
    descriptor = descriptor_allocate(write_end ? DESCRIPTOR_PIPE_WRITE :
                                                  DESCRIPTOR_PIPE_READ);
    if (descriptor) {
        descriptor->object.pipe = pipe;
        pipe_endpoint_open(pipe, direction);
    }
    return descriptor;
}

int descriptor_retain(struct descriptor *descriptor)
{
    if (!descriptor || descriptor->type == DESCRIPTOR_NONE ||
        descriptor->references == UINT32_MAX) {
        return -1;
    }
    descriptor->references++;
    return 0;
}

void descriptor_release(struct descriptor *descriptor)
{
    if (!descriptor || descriptor->type == DESCRIPTOR_NONE ||
        descriptor->references == 0u) {
        return;
    }
    descriptor->references--;
    if (descriptor->references != 0u) {
        return;
    }

    if (descriptor->type == DESCRIPTOR_VFS_FILE) {
        vfs_close(descriptor->object.file);
    } else if (descriptor->type == DESCRIPTOR_UDP_SOCKET) {
        net_udp_socket_close(descriptor->object.udp_socket);
    } else if (descriptor->type == DESCRIPTOR_TCP_SOCKET) {
        net_tcp_socket_close(descriptor->object.tcp_socket);
    } else if (descriptor->type == DESCRIPTOR_PIPE_READ) {
        pipe_endpoint_close(descriptor->object.pipe, PIPE_READ_END);
    } else if (descriptor->type == DESCRIPTOR_PIPE_WRITE) {
        pipe_endpoint_close(descriptor->object.pipe, PIPE_WRITE_END);
    }
    descriptor->object.file = 0;
    descriptor->type = DESCRIPTOR_NONE;
}

struct vfs_file *descriptor_vfs_file(struct descriptor *descriptor)
{
    if (!descriptor || descriptor->type != DESCRIPTOR_VFS_FILE) {
        return 0;
    }
    return descriptor->object.file;
}

struct udp_socket *descriptor_udp_socket(struct descriptor *descriptor)
{
    if (!descriptor || descriptor->type != DESCRIPTOR_UDP_SOCKET) {
        return 0;
    }
    return descriptor->object.udp_socket;
}

struct tcp_socket *descriptor_tcp_socket(struct descriptor *descriptor)
{
    if (!descriptor || descriptor->type != DESCRIPTOR_TCP_SOCKET) return 0;
    return descriptor->object.tcp_socket;
}

int descriptor_standard_fd(struct descriptor *descriptor)
{
    if (!descriptor || descriptor->type != DESCRIPTOR_STANDARD) {
        return -1;
    }
    return (int)descriptor->object.standard_fd;
}

struct pipe_object *descriptor_pipe(struct descriptor *descriptor,
                                    int *write_end)
{
    if (!descriptor ||
        (descriptor->type != DESCRIPTOR_PIPE_READ &&
         descriptor->type != DESCRIPTOR_PIPE_WRITE)) {
        return 0;
    }
    if (write_end) {
        *write_end = descriptor->type == DESCRIPTOR_PIPE_WRITE;
    }
    return descriptor->object.pipe;
}

size_t descriptor_live_count(void)
{
    size_t count = 0u;

    for (size_t index = 0; index < DESCRIPTOR_MAX_OBJECTS; index++) {
        if (descriptors[index].type != DESCRIPTOR_NONE) {
            count++;
        }
    }
    return count;
}
