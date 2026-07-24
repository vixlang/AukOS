#ifndef AUKOS_TIMER_H
#define AUKOS_TIMER_H

#include <stdint.h>

void timer_init(uint32_t frequency_hz);
uint64_t timer_ticks(void);
int timer_wait_for_ticks(uint64_t target_ticks, uint64_t spin_limit);

#endif
