#ifndef STM32_DEBUG_UART_H
#define STM32_DEBUG_UART_H

#include <stdint.h>

/*
 * 初始化USART1调试输出。
 * TX固定使用PA9，格式固定为115200、8数据位、无校验、1停止位。
 */
void debug_uart_init(uint32_t peripheral_clock_hz);
void debug_uart_puts(const char *text);
void debug_uart_put_u64(uint64_t value);
void debug_uart_flush(void);

#endif
