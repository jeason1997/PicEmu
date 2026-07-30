#include <xc.h>

/* 关闭看门狗，避免 Flash 写忙轮询期间发生非预期复位。 */
#pragma config WDTE = OFF

/*
 * PIC10F202没有硬件SPI，本例用普通GPIO模拟SPI Mode 0。
 *
 * GP0：W25Q的/CS片选
 * GP1：W25Q的CLK时钟
 * GP2：W25Q的DI（MOSI），校验结束后同时用来驱动LED
 * GP3：W25Q的DO（MISO），只能作为输入
 *
 * SPI Mode 0 的空闲时钟为低电平，数据在上升沿被采样、下降沿后准备下一位。
 */

#define PIN_CS   0x01
#define PIN_CLK  0x02
#define PIN_MOSI 0x04

/* 片选为低有效；宏同步更新 GPIO 输出影子值和实际端口。 */
#define FLASH_SELECT() do { \
    gpio_output &= (unsigned char)~PIN_CS; \
    GPIO = gpio_output; \
} while (0)
#define FLASH_DESELECT() do { \
    gpio_output |= PIN_CS; \
    GPIO = gpio_output; \
} while (0)

static unsigned char gpio_output = PIN_CS;

/* 写入和读回共用缓冲区，以节省 PIC10F202 十分有限的 RAM。 */
static unsigned char buffer[5];

/* 最高位优先地发送一个字节，同时在每个时钟上升沿采样 MISO。 */
static unsigned char spi_transfer(unsigned char value)
{
    unsigned char result = 0;
    unsigned char bit;

    for (bit = 0; bit < 8; ++bit) {
        /* CLK 为低时准备 MOSI，满足从设备上升沿采样前的数据建立时间。 */
        if ((value & 0x80) != 0) {
            gpio_output |= PIN_MOSI;
        } else {
            gpio_output &= (unsigned char)~PIN_MOSI;
        }
        GPIO = gpio_output;

        /* 拉高 CLK，并在同一个位周期内读取从设备输出的 GP3。 */
        gpio_output |= PIN_CLK;
        GPIO = gpio_output;
        result <<= 1;
        if (GP3 != 0) {
            result |= 1;
        }

        /* 拉低 CLK 回到 Mode 0 空闲电平，再移入下一位。 */
        gpio_output &= (unsigned char)~PIN_CLK;
        GPIO = gpio_output;
        value <<= 1;
    }
    return result;
}

/*
 * 向指定地址写入一段数据，不局限于示例字符串。
 * PIC10F202的资源很小，本教学接口使用16位地址，并要求单次数据不跨256字节页。
 * 调用者还应确保目标区域已擦除；W25Q 页编程只能把位从 1 写成 0。
 */
static void w25q_write(unsigned int address,
                       const unsigned char *data,
                       unsigned char length)
{
    unsigned char index;
    unsigned char status;

    /* 先发送 Write Enable（0x06），设置芯片内部的写使能锁存位。 */
    FLASH_SELECT();
    spi_transfer(0x06);
    FLASH_DESELECT();

    /* 页编程命令 0x02 后依次发送 24 位地址和待写数据。 */
    FLASH_SELECT();
    spi_transfer(0x02);
    spi_transfer(0x00);
    spi_transfer((unsigned char)(address >> 8));
    spi_transfer((unsigned char)address);
    for (index = 0; index < length; ++index) {
        spi_transfer(data[index]);
    }
    FLASH_DESELECT();

    /*
     * 写操作在片选拉高后仍由 Flash 内部继续执行。轮询状态寄存器 bit0
     * （BUSY），直到它清零后才允许调用者继续读取。
     */
    do {
        FLASH_SELECT();
        spi_transfer(0x05);
        status = spi_transfer(0x00);
        FLASH_DESELECT();
    } while ((status & 0x01) != 0);
}

/*
 * 从指定地址连续读取任意内容到调用者提供的缓冲区。
 * 读取命令 0x03 支持跨页连续读取，这里仍使用 16 位业务地址并补一个高地址 0。
 */
static void w25q_read(unsigned int address,
                      unsigned char *data,
                      unsigned char length)
{
    unsigned char index;

    /* 发送 Read Data 命令和 24 位起始地址。 */
    FLASH_SELECT();
    spi_transfer(0x03);
    spi_transfer(0x00);
    spi_transfer((unsigned char)(address >> 8));
    spi_transfer((unsigned char)address);
    for (index = 0; index < length; ++index) {
        /* 发送无意义的 0x00 只为产生时钟，返回值才是 Flash 输出的数据。 */
        data[index] = spi_transfer(0x00);
    }
    FLASH_DESELECT();
}

void main(void)
{
    /* 逐字节异或的结果会汇总到 mismatch；最终为 0 表示全部相等。 */
    unsigned char mismatch = 0;

    /* GP0/GP1/GP2 为 SPI 输出，GP3 为芯片固定的只输入引脚，用作 MISO。 */
    TRISGPIO = 0b111000;

    /* 初始保持 /CS 为高、CLK 和 MOSI 为低，Flash 未被选中。 */
    GPIO = gpio_output;

    /* 第一组测试：在地址 0 写入“Hello”，随后读回并逐字节校验。 */
    buffer[0] = 'H';
    buffer[1] = 'e';
    buffer[2] = 'l';
    buffer[3] = 'l';
    buffer[4] = 'o';
    w25q_write(0, buffer, sizeof(buffer));

    w25q_read(0, buffer, sizeof(buffer));
    /*
     * 使用“异或后按位或”的方式累计差异，不需要额外分支：
     * 任一字节不同都会让 mismatch 至少有一位为 1。
     */
    mismatch |= buffer[0] ^ 'H';
    mismatch |= buffer[1] ^ 'e';
    mismatch |= buffer[2] ^ 'l';
    mismatch |= buffer[3] ^ 'l';
    mismatch |= buffer[4] ^ 'o';

    /* 第二组测试：紧接地址 5 写入“World”，验证通用地址和长度接口。 */
    buffer[0] = 'W';
    buffer[1] = 'o';
    buffer[2] = 'r';
    buffer[3] = 'l';
    buffer[4] = 'd';
    w25q_write(5, buffer, sizeof(buffer));

    w25q_read(5, buffer, sizeof(buffer));
    mismatch |= buffer[0] ^ 'W';
    mismatch |= buffer[1] ^ 'o';
    mismatch |= buffer[2] ^ 'r';
    mismatch |= buffer[3] ^ 'l';
    mismatch |= buffer[4] ^ 'd';

    /*
     * SPI 操作结束后复用 GP2（MOSI）驱动 LED。/CS 保持高电平，防止
     * GP2 的 LED 状态被 Flash 误认为新的串行数据。
     */
    gpio_output = PIN_CS | (mismatch == 0 ? PIN_MOSI : 0);
    GPIO = gpio_output;

    for (;;) {
        /* 测试只运行一次；停在稳定状态，便于观察 LED 和 Flash 数据。 */
        NOP();
    }
}
