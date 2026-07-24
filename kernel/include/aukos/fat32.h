#ifndef AUKOS_FAT32_H
#define AUKOS_FAT32_H

#include "block.h"

int fat32_probe(struct block_device *device);
int fat32_available(void);
void fat32_run_selftest(void);

#endif
