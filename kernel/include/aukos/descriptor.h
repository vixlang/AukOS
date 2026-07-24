#ifndef AUKOS_DESCRIPTOR_H
#define AUKOS_DESCRIPTOR_H

#include <stddef.h>
#include <stdint.h>

#define DESCRIPTOR_MAX_OBJECTS 64u

struct udp_socket;
struct vfs_file;
struct pipe_object;

enum descriptor_type {
    DESCRIPTOR_NONE = 0,
    DESCRIPTOR_STANDARD,
    DESCRIPTOR_VFS_FILE,
    DESCRIPTOR_UDP_SOCKET,
    DESCRIPTOR_PIPE_READ,
    DESCRIPTOR_PIPE_WRITE,
};

struct descriptor {
    enum descriptor_type type;
    uint32_t references;
    union {
        struct vfs_file *file;
        struct udp_socket *udp_socket;
        struct pipe_object *pipe;
        uint32_t standard_fd;
    } object;
};

void descriptor_init(void);
struct descriptor *descriptor_create_vfs(struct vfs_file *file);
struct descriptor *descriptor_create_udp(struct udp_socket *socket);
struct descriptor *descriptor_create_standard(uint32_t standard_fd);
struct descriptor *descriptor_create_pipe(struct pipe_object *pipe,
                                          int write_end);
int descriptor_retain(struct descriptor *descriptor);
void descriptor_release(struct descriptor *descriptor);
struct vfs_file *descriptor_vfs_file(struct descriptor *descriptor);
struct udp_socket *descriptor_udp_socket(struct descriptor *descriptor);
int descriptor_standard_fd(struct descriptor *descriptor);
struct pipe_object *descriptor_pipe(struct descriptor *descriptor,
                                    int *write_end);
size_t descriptor_live_count(void);

#endif
