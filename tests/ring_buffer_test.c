#include <stdint.h>

#define CAPACITY 4

struct ring_buffer {
    uint32_t values[CAPACITY];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
};

static int push(struct ring_buffer *buffer, uint32_t value)
{
    if (buffer->count == CAPACITY) {
        return -1;
    }

    buffer->values[buffer->tail] = value;
    buffer->tail = (buffer->tail + 1) % CAPACITY;
    buffer->count++;
    return 0;
}

static int pop(struct ring_buffer *buffer, uint32_t *value)
{
    if (buffer->count == 0) {
        return -1;
    }

    *value = buffer->values[buffer->head];
    buffer->head = (buffer->head + 1) % CAPACITY;
    buffer->count--;
    return 0;
}

static int expect(int condition)
{
    return condition ? 0 : 1;
}

int main(void)
{
    struct ring_buffer buffer = {0};
    uint32_t value = 0;
    int failures = 0;

    failures += expect(pop(&buffer, &value) == -1);
    failures += expect(push(&buffer, 10) == 0);
    failures += expect(push(&buffer, 20) == 0);
    failures += expect(push(&buffer, 30) == 0);
    failures += expect(push(&buffer, 40) == 0);
    failures += expect(push(&buffer, 50) == -1);
    failures += expect(pop(&buffer, &value) == 0 && value == 10);
    failures += expect(pop(&buffer, &value) == 0 && value == 20);
    failures += expect(push(&buffer, 50) == 0);
    failures += expect(push(&buffer, 60) == 0);
    failures += expect(pop(&buffer, &value) == 0 && value == 30);
    failures += expect(pop(&buffer, &value) == 0 && value == 40);
    failures += expect(pop(&buffer, &value) == 0 && value == 50);
    failures += expect(pop(&buffer, &value) == 0 && value == 60);
    failures += expect(pop(&buffer, &value) == -1);

    return failures == 0 ? 0 : 1;
}
