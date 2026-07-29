#ifndef STM32F103_REGS_H
#define STM32F103_REGS_H

#include <stdint.h>

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t APB2RSTR;
    volatile uint32_t APB1RSTR;
    volatile uint32_t AHBENR;
    volatile uint32_t APB2ENR;
    volatile uint32_t APB1ENR;
} Stm32Rcc;

typedef struct {
    volatile uint32_t CRL;
    volatile uint32_t CRH;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t BRR;
} Stm32Gpio;

typedef struct {
    volatile uint32_t ACR;
} Stm32Flash;

#ifdef STM32_HOST_CHECK
static Stm32Rcc stm32_mock_rcc;
static Stm32Gpio stm32_mock_gpioa;
static Stm32Gpio stm32_mock_gpiob;
static Stm32Gpio stm32_mock_gpioc;
static Stm32Flash stm32_mock_flash;
static volatile uint32_t stm32_mock_demcr;
static volatile uint32_t stm32_mock_dwt_ctrl;
static volatile uint32_t stm32_mock_dwt_cyccnt;
#define RCC              (&stm32_mock_rcc)
#define GPIOA            (&stm32_mock_gpioa)
#define GPIOB            (&stm32_mock_gpiob)
#define GPIOC            (&stm32_mock_gpioc)
#define FLASH_REGS       (&stm32_mock_flash)
#define CORE_DEMCR       stm32_mock_demcr
#define DWT_CTRL         stm32_mock_dwt_ctrl
#define DWT_CYCCNT       stm32_mock_dwt_cyccnt
#else
#define RCC              ((Stm32Rcc *)0x40021000u)
#define GPIOA            ((Stm32Gpio *)0x40010800u)
#define GPIOB            ((Stm32Gpio *)0x40010C00u)
#define GPIOC            ((Stm32Gpio *)0x40011000u)
#define FLASH_REGS       ((Stm32Flash *)0x40022000u)
#define CORE_DEMCR       (*(volatile uint32_t *)0xE000EDFCu)
#define DWT_CTRL         (*(volatile uint32_t *)0xE0001000u)
#define DWT_CYCCNT       (*(volatile uint32_t *)0xE0001004u)
#endif

#define RCC_CR_HSION         (1u << 0)
#define RCC_CR_HSIRDY        (1u << 1)
#define RCC_CR_PLLON         (1u << 24)
#define RCC_CR_PLLRDY        (1u << 25)
#define RCC_CFGR_PLLMUL16    (14u << 18)
#define RCC_CFGR_PPRE1_DIV2  (4u << 8)
#define RCC_CFGR_SW_PLL      (2u << 0)
#define RCC_CFGR_SWS_PLL     (2u << 2)
#define RCC_APB2ENR_IOPAEN   (1u << 2)
#define RCC_APB2ENR_IOPBEN   (1u << 3)
#define RCC_APB2ENR_IOPCEN   (1u << 4)
#define FLASH_ACR_LATENCY_2  2u
#define FLASH_ACR_PRFTBE     (1u << 4)
#define CORE_DEMCR_TRCENA    (1u << 24)
#define DWT_CTRL_CYCCNTENA   (1u << 0)

#endif
