#include "debug_uart.h"
#include "stm32f103_regs.h"

#include <stdint.h>

#ifdef STM32_HOST_CHECK
static volatile uint32_t mock_usart1_sr = (1u << 7) | (1u << 6);
static volatile uint32_t mock_usart1_dr;
static volatile uint32_t mock_usart1_brr;
static volatile uint32_t mock_usart1_cr1;
#define USART1_SR  mock_usart1_sr
#define USART1_DR  mock_usart1_dr
#define USART1_BRR mock_usart1_brr
#define USART1_CR1 mock_usart1_cr1
#else
#define USART1_SR  (*(volatile uint32_t *)0x40013800u)
#define USART1_DR  (*(volatile uint32_t *)0x40013804u)
#define USART1_BRR (*(volatile uint32_t *)0x40013808u)
#define USART1_CR1 (*(volatile uint32_t *)0x4001380Cu)
#endif

#define RCC_APB2ENR_AFIOEN  (1u << 0)
#define RCC_APB2ENR_USART1EN (1u << 14)
#define USART_SR_TC          (1u << 6)
#define USART_SR_TXE         (1u << 7)
#define USART_CR1_TE         (1u << 3)
#define USART_CR1_UE         (1u << 13)
#define DEBUG_BAUD_RATE      115200u

void debug_uart_init(uint32_t peripheral_clock_hz)
{
    uint32_t config;

    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN |
                    RCC_APB2ENR_IOPAEN |
                    RCC_APB2ENR_USART1EN;

    /* PA9是USART1默认TX引脚：50MHz复用推挽输出。 */
    config = GPIOA->CRH;
    config &= ~(0xFu << 4);
    config |= 0xBu << 4;
    GPIOA->CRH = config;

    /*
     * STM32F1的USART BRR等于四舍五入后的PCLK/波特率。
     * 8MHz时为69（0x45），64MHz时为556（0x22C）。
     */
    USART1_BRR =
        (peripheral_clock_hz + DEBUG_BAUD_RATE / 2u) / DEBUG_BAUD_RATE;
    USART1_CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void debug_uart_putc(char character)
{
    while ((USART1_SR & USART_SR_TXE) == 0) {
    }
    USART1_DR = (uint32_t)(uint8_t)character;
}

void debug_uart_puts(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n') {
            debug_uart_putc('\r');
        }
        debug_uart_putc(*text++);
    }
}

void debug_uart_flush(void)
{
    /* 等最后一个停止位真正离开发送引脚。 */
    while ((USART1_SR & USART_SR_TC) == 0) {
    }
}
