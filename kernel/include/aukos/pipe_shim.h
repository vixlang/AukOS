#ifndef AUKOS_PIPE_SHIM_H
#define AUKOS_PIPE_SHIM_H

#include <stddef.h>
#include <stdint.h>

#include "pipe.h"

int pipe_shim_find_free(void);
struct pipe_object *pipe_shim_from_slot(int slot);

int pipe_shim_allocated(struct pipe_object *pipe);
void pipe_shim_set_allocated(struct pipe_object *pipe, int value);

uint32_t pipe_shim_readers(struct pipe_object *pipe);
void pipe_shim_set_readers(struct pipe_object *pipe, uint32_t value);

uint32_t pipe_shim_writers(struct pipe_object *pipe);
void pipe_shim_set_writers(struct pipe_object *pipe, uint32_t value);

size_t pipe_shim_count(struct pipe_object *pipe);
void pipe_shim_set_count(struct pipe_object *pipe, size_t value);

size_t pipe_shim_read_index(struct pipe_object *pipe);
void pipe_shim_set_read_index(struct pipe_object *pipe, size_t value);

size_t pipe_shim_write_index(struct pipe_object *pipe);
void pipe_shim_set_write_index(struct pipe_object *pipe, size_t value);

int pipe_shim_transfer_read(struct pipe_object *pipe, void *buffer,
                            size_t length, size_t *transferred);

int pipe_shim_transfer_write(struct pipe_object *pipe, const void *buffer,
                             size_t length, size_t *transferred);

int pipe_shim_live_count(void);

void pipe_shim_log(int level, const char *message);

#endif
