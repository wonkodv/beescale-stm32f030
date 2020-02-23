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


static void lora() {
    lora_selftest();

    lora_init();
    lora_set_frequency(LORA_FREQ_868M);
    lora_enable_crc();
    lora_send_packet((uint8_t*)"Hello", 6);

    uint8_t data[100];

    while(1) {
        lora_receive();
        while(!lora_available()){
            blinky();
            HAL_Delay(100);
        }
        int len = lora_receive_packet(data, sizeof(data));
        printf("Received %d: \n", len);
        if(len >= 0) {
            for(int i=0; i<len; i++) {
                printf("%c\t%2X\n", (data[i]<=20 ? '.' : data[i]), data[i]);
                HAL_Delay(50);
                blinky();
            }
        }
    }
}

/* called by main() after initialization is done */
void app(void)
{
    int i = 0;

    HAL_Delay(500); /* wait on make cat during debugging */

    printf("DEVID %lx\n", HAL_GetDEVID());
    printf("REVID %lx\n", HAL_GetREVID());
    printf("UID   %lx-%lx-%lx\n", HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2());

    lora();

    while(1) {
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin , 0);
        HAL_Delay(100);
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 1);

        // uint32_t adc;
        // assert(HAL_OK == HAL_ADC_Start(&hadc));
        // assert(HAL_OK == HAL_ADC_PollForConversion(&hadc, 10));
        // adc = HAL_ADC_GetValue(&hadc);
        // assert(HAL_OK == HAL_ADC_Stop(&hadc));
        // printf("%6lx \n", adc);
        HAL_Delay(100);
        i++;
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
