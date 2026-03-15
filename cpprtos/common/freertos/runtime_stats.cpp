#include "hardware/timer.h"

extern "C" uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
}