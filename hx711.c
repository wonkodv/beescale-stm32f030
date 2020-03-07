#include "gpio.h"
#include "main.h"
#include "hx711.h"

uint32_t hx711_read(void) {
    int timeout;
    uint32_t in=0;
    HAL_GPIO_WritePin(HX711_CLK_PD_GPIO_Port, HX711_CLK_PD_Pin , 1);

    timeout = 1000000;
    while(!HAL_GPIO_ReadPin(MISO_GPIO_Port, MISO_Pin)) {
        timeout--;
        if(!timeout) {
            return HX711_ILLEGAL;
        }
    }

    HAL_GPIO_WritePin(HX711_CLK_PD_GPIO_Port, HX711_CLK_PD_Pin , 0);
    timeout = 1000000;
    while(HAL_GPIO_ReadPin(MISO_GPIO_Port, MISO_Pin)) {
        timeout--;
        if(!timeout) {
            return HX711_TIMOUT;
        }
    }
    for(int j=0; j<24; j++) {
        in <<= 1;
        HAL_GPIO_WritePin(HX711_CLK_PD_GPIO_Port, HX711_CLK_PD_Pin , 1);
        HAL_GPIO_WritePin(HX711_CLK_PD_GPIO_Port, HX711_CLK_PD_Pin , 0);
        in |= HAL_GPIO_ReadPin(MISO_GPIO_Port, MISO_Pin);
    }
    HAL_GPIO_WritePin(HX711_CLK_PD_GPIO_Port, HX711_CLK_PD_Pin , 1);

    in += 0x800000;
    in &= 0xFFFFFF;

    return in;
}
