#include "gpio.h"
#include "lora.h"

#include "assert.h"

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
#define MODE_LORA                      0x80
#define MODE_LOW_FREQ                  0x08

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

/*********************** chip Communication ********************/

void lora_down(void) {
    HAL_GPIO_WritePin(CS_LORA_GPIO_Port, CS_LORA_Pin , 1);
    HAL_GPIO_WritePin(RES_LORA_GPIO_Port, RES_LORA_Pin, 0);
}

void lora_up(void) {
    HAL_GPIO_WritePin(RES_LORA_GPIO_Port, RES_LORA_Pin, 1);
}

static void spi_tx(uint8_t reg, uint8_t *buf, uint8_t count) {
    spi_start();
    HAL_Delay(1);
    (void)spi_byte(reg);
    for(int i=0; i<count; i++){
        buf[i] = spi_byte(buf[i]);
    }
    spi_stop();
}

static void lora_write_regs(uint8_t reg, uint8_t *buf, uint8_t count) {
    assert_comp(reg & 0x7F, ==, reg);
    spi_tx(reg|0x80, buf, count);
}

static void lora_write_reg(uint8_t reg, uint8_t val) {
    assert_comp(reg & 0x7F, ==, reg);
    spi_tx(reg|0x80, &val, 1);
}

static void lora_read_regs(uint8_t reg, uint8_t *buf, uint8_t count) {
    assert_comp(reg & 0x7F, ==, reg);
    spi_tx(reg, buf, count);
}

static uint8_t lora_read_reg(uint8_t reg) {
    assert_comp(reg & 0x7F, ==, reg);
    uint8_t val;
    spi_tx(reg, &val, 1);
    return val;
}

/*********************** Control *******************************/

void lora_init(void) {
    lora_reset();

    uint8_t version = lora_read_reg(REG_VERSION);
    if(version !=  0x12) {
        panic();
    }

    lora_sleep();

    lora_write_reg(REG_FIFO_RX_BASE_ADDR, 0);
    lora_write_reg(REG_FIFO_TX_BASE_ADDR, 0);
    lora_write_reg(REG_LNA, lora_read_reg(REG_LNA) | 0x03); //  TODO: this good? boost 150%LNA Current
    lora_write_reg(REG_MODEM_CONFIG_3, 0x04); // TODO: good? AgcOnAuto

    lora_idle();
}

void lora_reset(void) {
    lora_down();
    HAL_Delay(1);
    lora_up();
    HAL_Delay(10);
}

/*********************** Communicate ***************************/

/**
 * Send a packet.
 * @param buf Data to be sent
 * @param size Size of data.
 */
void lora_send_packet(void *buf, uint8_t size) {
    int i;

    uint8_t *pbuf = (uint8_t*)buf;

    /*
     * Transfer data to radio.
     */
    lora_write_reg(REG_OP_MODE, MODE_LORA | MODE_STDBY);
    lora_write_reg(REG_FIFO_ADDR_PTR, 0);

    for(i=0; i<size; i++) {
        lora_write_reg(REG_FIFO, *pbuf++);  /* TODO: use only 1 SPI Transmit*/
    }

    lora_write_reg(REG_PAYLOAD_LENGTH, size);

    /*
     * Start transmission and wait for conclusion.
     */
    lora_write_reg(REG_OP_MODE, MODE_LORA | MODE_TX);
    while((lora_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == 0) {
        HAL_Delay(1);
    }

    lora_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
}

/**
 * Read a received packet.
 * @param buf Buffer for the data.
 * @param size Available size in buffer (bytes).
 * @return Number of bytes received,
 *          LORA_ERR_NO_PACKET,
 *          LORA_ERR_CRC_ERR,
 */
int16_t lora_receive_packet(void *buf, uint8_t size) {
    int i, len = 0;

    uint8_t *pbuf = (uint8_t*)buf;

    /*
     * Check interrupts.
     */
    int irq = lora_read_reg(REG_IRQ_FLAGS);
    lora_write_reg(REG_IRQ_FLAGS, irq);

    if((irq & IRQ_RX_DONE_MASK) == 0) {
        return LORA_ERR_NO_PACKET;
    }

    if(irq & IRQ_PAYLOAD_CRC_ERROR_MASK) {
        return LORA_ERR_CRC_ERR;
    }

    /*
     * Find packet size.
     */
    lora_write_reg(REG_OP_MODE, MODE_LORA | MODE_STDBY);
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
        *pbuf++ = lora_read_reg(REG_FIFO); /* TODO: use only 1 SPI Transfer */
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

/*********************** MODES *********************************/

/**
 * Sets the radio transceiver in idle mode.
 * Must be used to change registers and access the FIFO.
 */
void lora_idle(void) {
    lora_write_reg(REG_OP_MODE, MODE_LORA | MODE_STDBY);
}

/**
 * Sets the radio transceiver in sleep mode.
 * Low power consumption and FIFO is lost.
 */
void lora_sleep(void) {
    lora_write_reg(REG_OP_MODE, MODE_LORA | MODE_SLEEP);
}

/**
 * Sets the radio transceiver in receive mode.
 * Incoming packets will be received.
 */
void lora_receive(void) {
    lora_write_reg(REG_OP_MODE, MODE_LORA | MODE_RX_CONTINUOUS);
}

/*********************** Config ********************************/

/* TODO: Explain configs better */

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
 * Configure power level for transmission
 * @param level 2-17, from least to most power
 */
void lora_set_tx_power(uint8_t level) {
    // RF9x module uses PA_BOOST pin
    assert_comp(level, >=, 2);
    assert_comp(level, <=, 17);

    lora_write_reg(REG_PA_CONFIG, PA_BOOST | (level - 2));
}

/**
 * Set carrier frequency.
 * @param freq Frequency in Hz
 */
void lora_set_frequency(uint32_t freq) {
    frequency = freq;

    uint64_t frf = ((uint64_t)freq << 19) / 32000000;

    /* TODO: use write_many */
    lora_write_reg(REG_FRF_MSB, (uint8_t)(frf >> 16));
    lora_write_reg(REG_FRF_MID, (uint8_t)(frf >> 8));
    lora_write_reg(REG_FRF_LSB, (uint8_t)(frf >> 0));
}

/**
 * Set spreading factor.
 * @param sf 6-12, Spreading factor to use.
 */
void lora_set_spreading_factor(uint8_t sf) {
    assert_comp(sf,  >= , 6);
    assert_comp(sf,  <= , 12);

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
 * Set bandwidth using LORA_BW_ constants.
 * Default LORA_BW_41700Hz
 */
void lora_set_bandwidth(uint8_t bw) {
    assert_comp(bw, <=, LORA_BW_MAX);
    lora_write_reg(REG_MODEM_CONFIG_1, (lora_read_reg(REG_MODEM_CONFIG_1) & 0x0f) | (bw << 4));
}

/**
 * Set coding rate
 * @param denominator 5-8, Denominator for the coding rate 4/x
 */
void lora_set_coding_rate(uint8_t denominator) {
    assert_comp(denominator,  >= , 5);
    assert_comp(denominator,  <= , 8);

    uint8_t cr = denominator - 4;
    lora_write_reg(REG_MODEM_CONFIG_1, (lora_read_reg(REG_MODEM_CONFIG_1) & 0xf1) | (cr << 1));
}

/**
 * Set the size of preamble.
 * @param length Preamble length in symbols.
 */
void lora_set_preamble_length(uint32_t length) {
    assert_comp(length,  >= , 4);
    assert_comp(length,  < , 0x100004);
    length = length - 4;
    lora_write_reg(REG_PREAMBLE_MSB, (uint8_t)(length >> 8));
    lora_write_reg(REG_PREAMBLE_LSB, (uint8_t)(length >> 0));
}

/**
 * Change radio sync word.
 * 34 Reserved for LoraWan
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


#if DEBUG==1

#define CHECK_REG(reg, val) do{             \
    lora_write_reg(reg, val);               \
    assert_comp(val,  == , lora_read_reg(reg));      \
}while(0)


/**
 * Test implementation and connection to SX127x by checking:
 * *    Version
 * *    Mode Switching
 * *    Default Values after reset
 * *    Fifo Operation
 */
void lora_selftest(){
    printf("Lora Selftest ...");
    lora_reset();

    /* Hardcoded Semtech Silicon Version */
    assert_comp(lora_read_reg(REG_VERSION),  == , 0x12);

    /* Mode after Reset is default */
    assert_comp(MODE_LOW_FREQ| MODE_STDBY, ==, lora_read_reg(REG_OP_MODE));

    /* Switch to Lora Stdby via Sleep works */
    CHECK_REG(REG_OP_MODE, MODE_LORA | MODE_SLEEP);
    CHECK_REG(REG_OP_MODE, MODE_LORA | MODE_STDBY);

    /* Lora Fifo Registers have deault Registers */
    assert_comp(0, == , lora_read_reg(REG_FIFO_RX_BASE_ADDR));
    assert_comp(0x80,   == , lora_read_reg(REG_FIFO_TX_BASE_ADDR));
    assert_comp(0,   == , lora_read_reg(REG_FIFO_ADDR_PTR));

    /* Writing and reading back Config registers works */
    CHECK_REG(REG_FIFO_RX_BASE_ADDR, 0);
    CHECK_REG(REG_FIFO_RX_BASE_ADDR, 42);
    CHECK_REG(REG_FIFO_RX_BASE_ADDR, 0xFF);

    CHECK_REG(REG_FIFO_TX_BASE_ADDR, 0);
    CHECK_REG(REG_FIFO_TX_BASE_ADDR, 42);
    CHECK_REG(REG_FIFO_TX_BASE_ADDR, 0xFF);

    CHECK_REG(REG_FIFO_ADDR_PTR, 0);
    CHECK_REG(REG_FIFO_ADDR_PTR, 42);
    CHECK_REG(REG_FIFO_ADDR_PTR, 0xFF);

    /* Set Addr Ptr to start of Buffer */
    lora_write_reg(REG_FIFO_ADDR_PTR, 0);
    {
        /* Write 5 Byte to FIFO */
        uint8_t data[5] = {10,20,30,42,255};
        lora_write_regs(REG_FIFO, data, 5);
    }

    /* Addr ptr auto incremented by 5 */
    assert_comp(5, ==, lora_read_reg(REG_FIFO_ADDR_PTR));

    /* Read back values sing single register- reads */
    lora_write_reg(REG_FIFO_ADDR_PTR, 0);
    assert_comp(10, ==, lora_read_reg(REG_FIFO));
    assert_comp(20, ==, lora_read_reg(REG_FIFO));
    assert_comp(30, ==, lora_read_reg(REG_FIFO));
    assert_comp(42, ==, lora_read_reg(REG_FIFO));
    assert_comp(255, ==, lora_read_reg(REG_FIFO));

    CHECK_REG(REG_FIFO_ADDR_PTR, 0);
    {
        /* Read back values with batch-read */
        uint8_t rdata[5] = {0,0,0,0,0};
        lora_read_regs(REG_FIFO, rdata, 5);

        assert_comp(10, ==, rdata[0]);
        assert_comp(20, ==, rdata[1]);
        assert_comp(30, ==, rdata[2]);
        assert_comp(42, ==, rdata[3]);
        assert_comp(255, ==, rdata[4]);
    }



    lora_reset();

    assert_comp(MODE_LOW_FREQ| MODE_STDBY, ==, lora_read_reg(REG_OP_MODE));
    CHECK_REG(REG_OP_MODE, MODE_LORA | MODE_SLEEP);
    CHECK_REG(REG_OP_MODE, MODE_LORA | MODE_STDBY);
    assert_comp(0, == , lora_read_reg(REG_FIFO_RX_BASE_ADDR));
    assert_comp(0x80,   == , lora_read_reg(REG_FIFO_TX_BASE_ADDR));
    assert_comp(0,   == , lora_read_reg(REG_FIFO_ADDR_PTR));

    printf("\b\b\bsuccessfull\n");
}
#else
void lora_selftest(){
    printf("Lora Selftest not implemented");
    assert(0);
}
#endif
