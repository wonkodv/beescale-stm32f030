#include <errno.h>
#include <stdio.h>
#include "gpio.h"
#include "main.h"
#include "lora.h"

#include "app.h"

extern UART_HandleTypeDef huart1;
extern ADC_HandleTypeDef hadc;


static void blinky() {
    static uint8_t b = 0;
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin , b);
    b = !b;
}



static struct {
    uint32_t devid;
    uint32_t vbat;
} Message;

void measure_vbat() {
    assert(HAL_OK == HAL_ADC_Start(&hadc));
    assert(HAL_OK == HAL_ADC_PollForConversion(&hadc, 10));
    Message.vbat = HAL_ADC_GetValue(&hadc);
    assert(HAL_OK == HAL_ADC_Stop(&hadc));
}

/* called by main() after initialization is done */
void app(void)
{
    HAL_Delay(500); /* wait on make cat during debugging */

    Message.devid = HAL_GetDEVID();
    printf("DEVID %lx\n", Message.devid);

#if DEBUG==1
    lora_selftest();
#endif

    lora_init();
    lora_set_frequency(LORA_FREQ_868M);
    lora_set_tx_power(2);
    // lora_set_spreading_factor(6);
    // lora_set_bandwidth(LORA_B);
    // lora_set_coding_rate(5);
    // lora_set_preamble_length(4);
    lora_enable_crc();


    while(1) {
        blinky();

#if 0
        measure_vbat();

        printf("0x%04lX\n", Message.vbat);
        lora_send_packet(&Message, sizeof(Message));
        lora_sleep();

        /* TODO: RTC Wake Up, es gibt Beispiele */
        HAL_Delay(1000);
#else
        lora_receive();
        while(!lora_available()){
            blinky();
            HAL_Delay(50);
        }
        int16_t len = lora_receive_packet(&Message, sizeof(Message));
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
                printf("Battery at    %4lumV (0x%4lX)\n", b, Message.vbat);
                break;
            }
            default:
                printf("can not happen: %d\n", len);
        }

        HAL_Delay(1000);

#endif
    }
    panic();
}

void panic(void) {
    while(1) {
        printf("panic");
        HAL_DeInit();
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
