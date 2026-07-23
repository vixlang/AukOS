#ifndef AUKOS_PIPE_H
#define AUKOS_PIPE_H

#include <stddef.h>
#include <stdint.h>

#define PIPE_MAX_OBJECTS 16u
#define PIPE_BUFFER_CAPACITY 4096u

enum pipe_direction {
    PIPE_READ_END = 0,
    PIPE_WRITE_END,
};

enum pipe_io_result {
    PIPE_IO_OK = 0,
    PIPE_IO_WOULD_BLOCK,
    PIPE_IO_EOF,
    PIPE_IO_BROKEN,
};

struct pipe_object {
    uint8_t buffer[PIPE_BUFFER_CAPACITY];
    size_t read_index;
    size_t write_index;
    size_t count;
    uint32_t readers;
    uint32_t writers;
    int allocated;
};

void pipe_init(void);
struct pipe_object *pipe_allocate(void);
void pipe_endpoint_open(struct pipe_object *pipe, enum pipe_direction direction);
void pipe_endpoint_close(struct pipe_object *pipe, enum pipe_direction direction);
enum pipe_io_result pipe_read(struct pipe_object *pipe, void *buffer,
                              size_t length, size_t *transferred);
enum pipe_io_result pipe_write(struct pipe_object *pipe, const void *buffer,
                               size_t length, size_t *transferred);
size_t pipe_data_size(const struct pipe_object *pipe);
size_t pipe_space_size(const struct pipe_object *pipe);
uint32_t pipe_reader_count(const struct pipe_object *pipe);
uint32_t pipe_writer_count(const struct pipe_object *pipe);
size_t pipe_live_count(void);

#endif
