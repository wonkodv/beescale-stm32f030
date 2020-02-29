#include "gpio.h"
#include "main.h"
#include "hx711.h"

uint32_t hx711_read(void) {
    uint32_t in=0;
    HAL_GPIO_WritePin(HX711_CLK_PD_GPIO_Port, HX711_CLK_PD_Pin , 0);
    for(int j=0; j<24; j++) {
        in <<= 1;
        HAL_GPIO_WritePin(HX711_CLK_PD_GPIO_Port, HX711_CLK_PD_Pin , 1);
        HAL_GPIO_WritePin(HX711_CLK_PD_GPIO_Port, HX711_CLK_PD_Pin , 0);
        in |= HAL_GPIO_ReadPin(SPI1_MISO_GPIO_Port, SPI1_MISO_Pin);
    }
    HAL_GPIO_WritePin(HX711_CLK_PD_GPIO_Port, HX711_CLK_PD_Pin , 1);
    return in;
}
