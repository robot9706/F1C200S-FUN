#include "arm32.h"

struct arm_regs_t {
    uint32_t r[13];
    uint32_t sp;
    uint32_t lr;
    uint32_t pc;
    uint32_t cpsr;
};

void _undefined_instruction_(struct arm_regs_t* regs) {
    while(1)
        ;
}

void _software_interrupt_(struct arm_regs_t* regs) {
}

void _prefetch_abort_(struct arm_regs_t* regs) {
    while(1)
        ;
}

void _data_abort_(struct arm_regs_t* regs) {
    while(1)
        ;
}
