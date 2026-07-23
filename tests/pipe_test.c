#include <aukos/pipe.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void open_pair(struct pipe_object *pipe)
{
    pipe_endpoint_open(pipe, PIPE_READ_END);
    pipe_endpoint_open(pipe, PIPE_WRITE_END);
}

int main(void)
{
    uint8_t input[PIPE_BUFFER_CAPACITY];
    uint8_t output[PIPE_BUFFER_CAPACITY];
    struct pipe_object *allocated[PIPE_MAX_OBJECTS];
    struct pipe_object *pipe;
    size_t moved;

    for (size_t index = 0; index < sizeof(input); index++) {
        input[index] = (uint8_t)(index * 17u + 3u);
    }

    pipe_init();
    pipe = pipe_allocate();
    assert(pipe != NULL);
    open_pair(pipe);
    assert(pipe_read(pipe, output, 1u, &moved) == PIPE_IO_WOULD_BLOCK);
    assert(pipe_write(pipe, input, sizeof(input), &moved) == PIPE_IO_OK);
    assert(moved == sizeof(input));
    assert(pipe_write(pipe, input, 1u, &moved) == PIPE_IO_WOULD_BLOCK);
    assert(pipe_read(pipe, output, 3000u, &moved) == PIPE_IO_OK);
    assert(moved == 3000u && memcmp(output, input, moved) == 0);

    assert(pipe_write(pipe, input, 3000u, &moved) == PIPE_IO_OK);
    assert(moved == 3000u);
    assert(pipe_data_size(pipe) == PIPE_BUFFER_CAPACITY);
    assert(pipe_read(pipe, output, sizeof(output), &moved) == PIPE_IO_OK);
    assert(moved == sizeof(output));
    assert(memcmp(output, input + 3000u, sizeof(input) - 3000u) == 0);
    assert(memcmp(output + sizeof(input) - 3000u, input, 3000u) == 0);

    pipe_endpoint_close(pipe, PIPE_WRITE_END);
    assert(pipe_read(pipe, output, 1u, &moved) == PIPE_IO_EOF);
    pipe_endpoint_close(pipe, PIPE_READ_END);
    assert(pipe_live_count() == 0u);

    pipe = pipe_allocate();
    open_pair(pipe);
    pipe_endpoint_close(pipe, PIPE_READ_END);
    assert(pipe_write(pipe, input, 1u, &moved) == PIPE_IO_BROKEN);
    pipe_endpoint_close(pipe, PIPE_WRITE_END);

    for (size_t index = 0; index < PIPE_MAX_OBJECTS; index++) {
        allocated[index] = pipe_allocate();
        assert(allocated[index] != NULL);
        open_pair(allocated[index]);
    }
    assert(pipe_allocate() == NULL);
    for (size_t index = 0; index < PIPE_MAX_OBJECTS; index++) {
        pipe_endpoint_close(allocated[index], PIPE_READ_END);
        pipe_endpoint_close(allocated[index], PIPE_WRITE_END);
    }
    assert(pipe_live_count() == 0u);
    assert(pipe_allocate() != NULL);

    puts("pipe_test: PASS");
    return 0;
}
