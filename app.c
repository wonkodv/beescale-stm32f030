#include <errno.h>
#include <stdio.h>

#include "gpio.h"
#include "main.h"
#include "adc.h"
#include "usart.h"

#include "lora.h"
#include "hx711.h"

#include "assert.h"

void print(char * s);

static void blinky() {
    static uint8_t b = 0;
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin , b);
    b = !b;
}

static volatile struct {
    uint32_t devid;
    uint32_t weight;
    uint16_t vbat;
    uint16_t vtemp;
    uint16_t vref;
} Message;

static void measure() {
    Message.vref = 0xFFFF;
    assert(HAL_ADC_Start_DMA(&hadc, (uint32_t*)&Message.vbat, 3) == HAL_OK);

    Message.weight = hx711_read();

    int timeout = 100;
    while(Message.vref == 0xFFFFu) {
        assert(timeout--);
    }
}

static void print_message() {
    printf("DevId  %8lX ", Message.devid);
    printf("Weight %8lX ", Message.weight);
    printf("VBat   %4X ", Message.vbat);
    printf("vtemp  %4X ", Message.vtemp);
    printf("vref   %4X ", Message.vref);
    printf("\n");
}


#pragma GCC diagnostic ignored "-Wunused-function"
static void app_lora_test(void) {
    lora_selftest();
    print("Lora OK");
    while(1) {
        blinky();
        HAL_Delay(1000);
    }
}

static void app_measure_test(void) {
    assert(HAL_OK == HAL_ADCEx_Calibration_Start(&hadc));
    while(1) {
        blinky();
        measure();
        print_message();
    }
}

static void app_send_measurements(void) {
    lora_init();
    lora_set_frequency(LORA_FREQ_868M);
#if 0
    lora_set_header_mode(LORA_HEADER_EXPLICIT);       /* TODO                                     */
    lora_set_tx_power(17);                            /* TODO                                     */
    lora_set_spreading_factor(6);
    lora_set_bandwidth(LORA_BW_125kHz);
    lora_set_coding_rate(5);
    lora_set_preamble_length(4);
    lora_enable_crc();
#endif
    assert(HAL_OK == HAL_ADCEx_Calibration_Start(&hadc));
    while(1) {
        blinky();
        measure();
        print_message();
        lora_send_packet((void*)&Message, sizeof(Message));
        lora_sleep();
        HAL_Delay(1000);
    }
}

static void app_lora_receiver(void) {
    lora_receive();
    while(1) {
        blinky();
        while(!lora_available()){
            blinky();
            HAL_Delay(50);
        }
        int16_t len = lora_receive_packet((void*)&Message, sizeof(Message));
        lora_sleep();

        switch(len) {
            case LORA_ERR_NO_PACKET:
                printf("No Packet");
                break;
            case LORA_ERR_CRC_ERR:
                printf("CRC Error\n");
                /* no break */
            case sizeof(Message): {
                printf("From    0x%08lX\n", Message.devid);
                uint32_t b = (Message.vbat * 2800 * 2) / 0x800;
                printf("Battery at    %4lumV (0x%4X)\n", b, Message.vbat);
                break;
            }
            default:
                printf("can not happen: %d\n", len);
        }
        HAL_Delay(1000);
    }
}


/* called by main() after initialization is done */
void app(void)
{
    HAL_Delay(500); /* wait on make cat during debugging */
    Message.devid = HAL_GetUIDw2();


    app_send_measurements();
    // app_measure_test();
    panic();
}

void print(char * s){
    if(s) {
        char * p = s;
        while(*p) {
            HAL_UART_Transmit(&huart1, (uint8_t*)p, 1, 10);
            p++;
        }
        HAL_UART_Transmit(&huart1, (uint8_t*)"\n", 1, 10);
    }
}

void panic(void) {
    while(1) {
        HAL_UART_Transmit(&huart1, (uint8_t*)"PANIC !", 8, 10);
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    }
}

int _write(int fd, uint8_t *data, size_t size) {
    if(fd == 1) {
        HAL_UART_Transmit(&huart1, data, size, 10*size);
        return size;
    }
    return EBADF;
}
