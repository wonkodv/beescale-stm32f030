#include "gpio.h"
#include "app.h"
#include "lora.h"

/* a lot Copied from here:
 * https://github.com/Inteform/PyLora/blob/master/src/lora.c
 */

/*
 * Register definitions
 */
#define REG_FIFO                       0x00
#define REG_OP_MODE                    0x01
#define REG_FRF_MSB                    0x06
#define REG_FRF_MID                    0x07
#define REG_FRF_LSB                    0x08
#define REG_PA_CONFIG                  0x09
#define REG_LNA                        0x0c
#define REG_FIFO_ADDR_PTR              0x0d
#define REG_FIFO_TX_BASE_ADDR          0x0e
#define REG_FIFO_RX_BASE_ADDR          0x0f
#define REG_FIFO_RX_CURRENT_ADDR       0x10
#define REG_IRQ_FLAGS_MASK             0x11
#define REG_IRQ_FLAGS                  0x12
#define REG_RX_NB_BYTES                0x13
#define REG_PKT_SNR_VALUE              0x19
#define REG_PKT_RSSI_VALUE             0x1a
#define REG_MODEM_CONFIG_1             0x1d
#define REG_MODEM_CONFIG_2             0x1e
#define REG_PREAMBLE_MSB               0x20
#define REG_PREAMBLE_LSB               0x21
#define REG_PAYLOAD_LENGTH             0x22
#define REG_MODEM_CONFIG_3             0x26
#define REG_RSSI_WIDEBAND              0x2c
#define REG_DETECTION_OPTIMIZE         0x31
#define REG_DETECTION_THRESHOLD        0x37
#define REG_SYNC_WORD                  0x39
#define REG_DIO_MAPPING_1              0x40
#define REG_VERSION                    0x42

/*
 * Transceiver modes
 */
#define MODE_LONG_RANGE_MODE           0x80
#define MODE_SLEEP                     0x00
#define MODE_STDBY                     0x01
#define MODE_TX                        0x03
#define MODE_RX_CONTINUOUS             0x05
#define MODE_RX_SINGLE                 0x06

/*
 * PA configuration
 */
#define PA_BOOST                       0x80

/*
 * IRQ masks
 */
#define IRQ_TX_DONE_MASK               0x08
#define IRQ_PAYLOAD_CRC_ERROR_MASK     0x20
#define IRQ_RX_DONE_MASK               0x40

static uint8_t implicit_header_mode;
static uint32_t frequency;


static void spi_start(void){
    HAL_GPIO_WritePin(CS_LORA_GPIO_Port, CS_LORA_Pin , 0);
}

static void spi_stop(void){
    HAL_GPIO_WritePin(CS_LORA_GPIO_Port, CS_LORA_Pin , 1);
}

static uint8_t spi_byte(uint8_t out){
    uint8_t in = 0;
    uint8_t o,i;

    for(int j=0; j<8; j++) {

        i = HAL_GPIO_ReadPin(SPI1_MISO_GPIO_Port, SPI1_MISO_Pin);
        in <<= 1;
        in |= i;
        o = out & 0x80 ? 1 : 0;
        out <<= 1;
        HAL_GPIO_WritePin(SPI1_MOSI_GPIO_Port, SPI1_MOSI_Pin , o);

        HAL_GPIO_WritePin(SPI1_CLK_GPIO_Port, SPI1_CLK_Pin , 1);
        HAL_GPIO_WritePin(SPI1_CLK_GPIO_Port, SPI1_CLK_Pin , 0);
    }
    return in;
}

void lora_reset(void) {
    printf("lora_reset()\n");
    HAL_GPIO_WritePin(CS_LORA_GPIO_Port, CS_LORA_Pin , 1);
    HAL_GPIO_WritePin(RES_LORA_GPIO_Port, RES_LORA_Pin, 0);
    HAL_Delay(1);
    HAL_GPIO_WritePin(RES_LORA_GPIO_Port, RES_LORA_Pin, 1);
    HAL_Delay(5);
}

void lora_down(void) {
    printf("lora_down()\n");
    HAL_GPIO_WritePin(RES_LORA_GPIO_Port, RES_LORA_Pin, 0);
    HAL_Delay(1);
}

void lora_up(void) {
    printf("lora_up()\n");
    HAL_GPIO_WritePin(RES_LORA_GPIO_Port, RES_LORA_Pin, 1);
}

static uint8_t lora_write_reg(uint8_t reg, uint8_t val) {
    uint8_t addr;
    uint8_t old;

    printf("lora_write_reg(%x, %x)\n", reg, val);

    spi_start();
    HAL_Delay(1);

    addr = reg | 0x80;

    (void)spi_byte(addr);

    old = spi_byte(val);

    spi_stop();

    return old;
}
static uint8_t lora_read_reg(uint8_t reg) {
    uint8_t addr;
    uint8_t val;

    spi_start();
    HAL_Delay(1);

    addr = reg & 0x7F;

    (void)spi_byte(addr);

    val = spi_byte(0);

    spi_stop();

    printf("lora_read_reg(%x) = %x\n", reg, val);

    return val;
}

/**
 * Configure explicit header mode.
 * Packet size will be included in the frame.
 */
void lora_explicit_header_mode(void) {
    implicit_header_mode = 0;
    lora_write_reg(REG_MODEM_CONFIG_1, lora_read_reg(REG_MODEM_CONFIG_1) & 0xfe);
}

/**
 * Configure implicit header mode.
 * All packets will have a predefined size.
 * @param size Size of the packets.
 */
void lora_implicit_header_mode(uint8_t size) {
    implicit_header_mode = 1;
    lora_write_reg(REG_MODEM_CONFIG_1, lora_read_reg(REG_MODEM_CONFIG_1) | 0x01);
    lora_write_reg(REG_PAYLOAD_LENGTH, size);
}

/**
 * Sets the radio transceiver in idle mode.
 * Must be used to change registers and access the FIFO.
 */
void lora_idle(void) {
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
}

/**
 * Sets the radio transceiver in sleep mode.
 * Low power consumption and FIFO is lost.
 */
void lora_sleep(void) {
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
}

/**
 * Sets the radio transceiver in receive mode.
 * Incoming packets will be received.
 */
void lora_receive(void) {
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
}

/**
 * Configure power level for transmission
 * @param level 2-17, from least to most power
 */
void lora_set_tx_power(uint8_t level) {
    // RF9x module uses PA_BOOST pin
    assert(level>=2);
    assert(level<=17);

    lora_write_reg(REG_PA_CONFIG, PA_BOOST | (level - 2));
}

/**
 * Set carrier frequency.
 * @param freq Frequency in Hz
 */
void lora_set_frequency(uint32_t freq) {
    frequency = freq;

    uint64_t frf = ((uint64_t)freq << 19) / 32000000;

    lora_write_reg(REG_FRF_MSB, (uint8_t)(frf >> 16));
    lora_write_reg(REG_FRF_MID, (uint8_t)(frf >> 8));
    lora_write_reg(REG_FRF_LSB, (uint8_t)(frf >> 0));
}

/**
 * Set spreading factor.
 * @param sf 6-12, Spreading factor to use.
 */
void lora_set_spreading_factor(uint8_t sf) {
    assert(sf >= 6);
    assert(sf <= 12);

    if (sf == 6) {
        lora_write_reg(REG_DETECTION_OPTIMIZE, 0xc5);
        lora_write_reg(REG_DETECTION_THRESHOLD, 0x0c);
    } else {
        lora_write_reg(REG_DETECTION_OPTIMIZE, 0xc3);
        lora_write_reg(REG_DETECTION_THRESHOLD, 0x0a);
    }

    lora_write_reg(REG_MODEM_CONFIG_2, (lora_read_reg(REG_MODEM_CONFIG_2) & 0x0f) | ((sf << 4) & 0xf0));
}

/**
 * Set bandwidth (bit rate)
 * @param sbw Bandwidth in Hz (up to 500000)
 */
void lora_set_bandwidth(uint32_t sbw) {
    int bw;

    assert(0); // untested

    switch(sbw) {
        case 7800: bw = 0; break;
        case 10400: bw = 1; break;
        case 15600: bw = 2; break;
        case 20800: bw = 3; break;
        case 31250: bw = 4; break;
        case 41700: bw = 5; break;
        case 62500: bw = 6; break;
        case 125000: bw = 7; break;
        case 250000: bw = 8; break;
        case 500000: bw = 9; break;
        default: assert(0); return;
    }
    lora_write_reg(REG_MODEM_CONFIG_1, (lora_read_reg(REG_MODEM_CONFIG_1) & 0x0f) | (bw << 4));
}

/**
 * Set coding rate
 * @param denominator 5-8, Denominator for the coding rate 4/x
 */
void lora_set_coding_rate(uint8_t denominator) {
    assert(denominator >= 5);
    assert(denominator <= 8);

    uint8_t cr = denominator - 4;
    lora_write_reg(REG_MODEM_CONFIG_1, (lora_read_reg(REG_MODEM_CONFIG_1) & 0xf1) | (cr << 1));
}

/**
 * Set the size of preamble.
 * @param length Preamble length in symbols.
 */
void lora_set_preamble_length(uint16_t length) {
    lora_write_reg(REG_PREAMBLE_MSB, (uint8_t)(length >> 8));
    lora_write_reg(REG_PREAMBLE_LSB, (uint8_t)(length >> 0));
}

/**
 * Change radio sync word.
 * @param sw New sync word to use.
 */
void lora_set_sync_word(uint8_t sw) {
    lora_write_reg(REG_SYNC_WORD, sw);
}

/**
 * Enable appending/verifying packet CRC.
 */
void lora_enable_crc(void) {
    lora_write_reg(REG_MODEM_CONFIG_2, lora_read_reg(REG_MODEM_CONFIG_2) | 0x04);
}

/**
 * Disable appending/verifying packet CRC.
 */
void lora_disable_crc(void) {
    lora_write_reg(REG_MODEM_CONFIG_2, lora_read_reg(REG_MODEM_CONFIG_2) & 0xfb);
}

/**
 * Perform hardware initialization.
 */
void lora_init(void) {
    /*
     * Perform hardware reset.
     */
    lora_reset();

    /*
     * Check version.
     */
    uint8_t version = lora_read_reg(REG_VERSION);
    assert(version == 0x12);

    /*
     * Default configuration.
     */
    lora_sleep();

    lora_write_reg(REG_FIFO_RX_BASE_ADDR, 0);
    lora_write_reg(REG_FIFO_TX_BASE_ADDR, 0);
    lora_write_reg(REG_LNA, lora_read_reg(REG_LNA) | 0x03); //  TODO: this good? boost 150%LNA Current
    lora_write_reg(REG_MODEM_CONFIG_3, 0x04); // TODO: good? AgcOnAuto

    lora_set_tx_power(17);

    lora_idle();
}

/**
 * Send a packet.
 * @param buf Data to be sent
 * @param size Size of data.
 */
void lora_send_packet(uint8_t *buf, uint8_t size) {
    int i;

    /*
     * Transfer data to radio.
     */
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
    lora_write_reg(REG_FIFO_ADDR_PTR, 0);

    for(i=0; i<size; i++) {
        lora_write_reg(REG_FIFO, *buf++);  /* TODO: use only 1 SPI Transmit*/
    }

    lora_write_reg(REG_PAYLOAD_LENGTH, size);

    /*
     * Start transmission and wait for conclusion.
     */
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);
    while((lora_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == 0) {
        HAL_Delay(1);
    }

    lora_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
}

/**
 * Read a received packet.
 * @param buf Buffer for the data.
 * @param size Available size in buffer (bytes).
 * @return Number of bytes received (zero if no packet available).
 */
uint8_t lora_receive_packet(uint8_t *buf, uint8_t size) {
    int i, len = 0;

    /*
     * Check interrupts.
     */
    int irq = lora_read_reg(REG_IRQ_FLAGS);
    lora_write_reg(REG_IRQ_FLAGS, irq);
    if((irq & IRQ_RX_DONE_MASK) == 0) {
        return 0; // TODO: warum
    }
    if(irq & IRQ_PAYLOAD_CRC_ERROR_MASK) {
        return 0;
    }

    /*
     * Find packet size.
     */
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
    if (implicit_header_mode) {
        len = lora_read_reg(REG_PAYLOAD_LENGTH);
    } else {
        len = lora_read_reg(REG_RX_NB_BYTES);
    }

    /*
     * Transfer data from radio.
     */
    lora_write_reg(REG_FIFO_ADDR_PTR, lora_read_reg(REG_FIFO_RX_CURRENT_ADDR));
    if(len > size) {
        len = size;
    }
    for(i=0; i<len; i++) {
        *buf++ = lora_read_reg(REG_FIFO); /* TODO: use only 1 SPI Transfer */
    }
    return len;
}

/**
 * Returns non-zero if there is data to read (packet received).
 */
uint8_t lora_available(void) {
    int m = lora_read_reg(REG_IRQ_FLAGS) & IRQ_RX_DONE_MASK;
    if(m) {
        return 1;
    }
    return 0;
}

/**
 * Return last packet's RSSI.
 */
uint8_t lora_packet_rssi(void) {
    uint8_t v = lora_read_reg(REG_PKT_RSSI_VALUE);
    return v - (frequency < LORA_FREQ_868M ? 164 : 157);
}

/**
 * Return last packet's SNR (signal to noise ratio).
 */
float lora_packet_snr(void) {
    int v = lora_read_reg(REG_PKT_SNR_VALUE);
    return ((int8_t)v) * 0.25;
}
