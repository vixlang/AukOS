#include "include/aukos/pipe.h"

static struct pipe_object pipes[PIPE_MAX_OBJECTS];

void pipe_init(void)
{
    for (size_t index = 0; index < PIPE_MAX_OBJECTS; index++) {
        pipes[index].allocated = 0;
        pipes[index].readers = 0;
        pipes[index].writers = 0;
        pipes[index].count = 0;
        pipes[index].read_index = 0;
        pipes[index].write_index = 0;
    }
}

struct pipe_object *pipe_allocate(void)
{
    for (size_t index = 0; index < PIPE_MAX_OBJECTS; index++) {
        struct pipe_object *pipe = &pipes[index];

        if (pipe->allocated) {
            continue;
        }
        pipe->allocated = 1;
        pipe->readers = 0;
        pipe->writers = 0;
        pipe->count = 0;
        pipe->read_index = 0;
        pipe->write_index = 0;
        return pipe;
    }
    return 0;
}

void pipe_endpoint_open(struct pipe_object *pipe, enum pipe_direction direction)
{
    if (!pipe || !pipe->allocated) {
        return;
    }
    if (direction == PIPE_READ_END) {
        pipe->readers++;
    } else {
        pipe->writers++;
    }
}

void pipe_endpoint_close(struct pipe_object *pipe, enum pipe_direction direction)
{
    if (!pipe || !pipe->allocated) {
        return;
    }
    if (direction == PIPE_READ_END) {
        if (pipe->readers != 0u) {
            pipe->readers--;
        }
    } else if (pipe->writers != 0u) {
        pipe->writers--;
    }
    if (pipe->readers == 0u && pipe->writers == 0u) {
        pipe->allocated = 0;
        pipe->count = 0;
    }
}

enum pipe_io_result pipe_read(struct pipe_object *pipe, void *buffer,
                              size_t length, size_t *transferred)
{
    uint8_t *bytes = buffer;
    size_t amount;

    if (transferred) {
        *transferred = 0;
    }
    if (!pipe || !pipe->allocated || !buffer || !transferred) {
        return PIPE_IO_BROKEN;
    }
    if (length == 0u) {
        return PIPE_IO_OK;
    }
    if (pipe->count == 0u) {
        return pipe->writers == 0u ? PIPE_IO_EOF : PIPE_IO_WOULD_BLOCK;
    }

    amount = length < pipe->count ? length : pipe->count;
    for (size_t index = 0; index < amount; index++) {
        bytes[index] = pipe->buffer[pipe->read_index];
        pipe->read_index = (pipe->read_index + 1u) % PIPE_BUFFER_CAPACITY;
    }
    pipe->count -= amount;
    *transferred = amount;
    return PIPE_IO_OK;
}

enum pipe_io_result pipe_write(struct pipe_object *pipe, const void *buffer,
                               size_t length, size_t *transferred)
{
    const uint8_t *bytes = buffer;
    size_t space;
    size_t amount;

    if (transferred) {
        *transferred = 0;
    }
    if (!pipe || !pipe->allocated || !buffer || !transferred ||
        pipe->readers == 0u) {
        return PIPE_IO_BROKEN;
    }
    if (length == 0u) {
        return PIPE_IO_OK;
    }
    space = PIPE_BUFFER_CAPACITY - pipe->count;
    if (space == 0u) {
        return PIPE_IO_WOULD_BLOCK;
    }

    amount = length < space ? length : space;
    for (size_t index = 0; index < amount; index++) {
        pipe->buffer[pipe->write_index] = bytes[index];
        pipe->write_index = (pipe->write_index + 1u) % PIPE_BUFFER_CAPACITY;
    }
    pipe->count += amount;
    *transferred = amount;
    return PIPE_IO_OK;
}

size_t pipe_data_size(const struct pipe_object *pipe)
{
    return pipe && pipe->allocated ? pipe->count : 0u;
}

size_t pipe_space_size(const struct pipe_object *pipe)
{
    return pipe && pipe->allocated ? PIPE_BUFFER_CAPACITY - pipe->count : 0u;
}

uint32_t pipe_reader_count(const struct pipe_object *pipe)
{
    return pipe && pipe->allocated ? pipe->readers : 0u;
}

uint32_t pipe_writer_count(const struct pipe_object *pipe)
{
    return pipe && pipe->allocated ? pipe->writers : 0u;
}

size_t pipe_live_count(void)
{
    size_t count = 0;

    for (size_t index = 0; index < PIPE_MAX_OBJECTS; index++) {
        if (pipes[index].allocated) {
            count++;
        }
    }
    return count;
}
