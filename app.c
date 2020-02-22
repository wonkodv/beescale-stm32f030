#include <errno.h>
#include <stdio.h>

#include "gpio.h"

extern UART_HandleTypeDef huart1;
extern ADC_HandleTypeDef hadc;


#define PIN_LED_1 GPIO_PIN_4

/* called by main() after initialization is done */
void app(void)
{
    int i = 0;

    printf("DEVID %lx\n", HAL_GetDEVID());
    printf("REVID %lx\n", HAL_GetREVID());
    printf("UID   %lx %lx %lx\n", HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2());

    while(1) {
        HAL_GPIO_WritePin(LED_GPIO_Port, PIN_LED_1 , 0);
        HAL_Delay(100);
        HAL_GPIO_WritePin(LED_GPIO_Port, PIN_LED_1, 1);

        HAL_ADC_Start(&hadc);
        HAL_ADC_PollForConversion(&hadc, 100);
        uint32_t adc = HAL_ADC_GetValue(&hadc);
        HAL_ADC_Stop(&hadc);

        printf("%3d\t %#4lx \n", i, adc);

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

