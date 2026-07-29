#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

int main(void);

static void Default_Handler(void)
{
    for (;;) {
    }
}

void Reset_Handler(void)
{
    uint32_t *source = &_sidata;
    uint32_t *target;

    for (target = &_sdata; target < &_edata;) {
        *target++ = *source++;
    }
    for (target = &_sbss; target < &_ebss;) {
        *target++ = 0;
    }

    (void)main();
    for (;;) {
    }
}

typedef void (*Handler)(void);

typedef struct {
    void *initial_stack;
    Handler reset;
    Handler nmi;
    Handler hard_fault;
    Handler mem_manage;
    Handler bus_fault;
    Handler usage_fault;
    Handler reserved_7;
    Handler reserved_8;
    Handler reserved_9;
    Handler reserved_10;
    Handler sv_call;
    Handler debug_monitor;
    Handler reserved_13;
    Handler pend_sv;
    Handler sys_tick;
} VectorTable;

__attribute__((used, section(".isr_vector")))
static const VectorTable VECTOR_TABLE = {
    .initial_stack = &_estack,
    .reset = Reset_Handler,
    .nmi = Default_Handler,
    .hard_fault = Default_Handler,
    .mem_manage = Default_Handler,
    .bus_fault = Default_Handler,
    .usage_fault = Default_Handler,
    .sv_call = Default_Handler,
    .debug_monitor = Default_Handler,
    .pend_sv = Default_Handler,
    .sys_tick = Default_Handler
};
