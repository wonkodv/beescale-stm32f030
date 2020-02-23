#include <errno.h>
#include <stdio.h>
#include "gpio.h"
#include "main.h"
#include "lora.h"


extern UART_HandleTypeDef huart1;
extern ADC_HandleTypeDef hadc;



static void lora() {
    lora_init();
    lora_set_frequency(915000000);
    lora_enable_crc();
    lora_send_packet((uint8_t*)"Hello", 6);

    printf("maybe sent successfully \n");
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
}



int _write(int fd, uint8_t *data, size_t size) {
    if(fd == 1) {
        HAL_UART_Transmit(&huart1, data, size, 10*size);
        return size;
    }
    return EBADF;
}

