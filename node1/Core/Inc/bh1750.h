/*
 * bh1750.h
 *
 *  Created on: 24 thg 3, 2026
 *      Author: khanh
 */
#ifndef INC_BH1750_H_
#define INC_BH1750_H_

#include "stm32f1xx_hal.h"

// Địa chỉ (ADDR = GND: 0x23, ADDR = VCC: 0x5C)
#define BH1750_ADDR_LOW   (0x23 << 1)
#define BH1750_ADDR_HIGH  (0x5C << 1)

// Mode đo
typedef enum {
    BH1750_CONT_HIRES   = 0x10, // 1 lux, 120ms
    BH1750_CONT_HIRES2  = 0x11, // 0.5 lux, 120ms
    BH1750_CONT_LORES   = 0x13, // 4 lux, 16ms
    BH1750_ONE_HIRES    = 0x20,
    BH1750_ONE_HIRES2   = 0x21,
    BH1750_ONE_LORES    = 0x23
} BH1750_Mode;

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t address;
    BH1750_Mode mode;
} BH1750_HandleTypeDef;

// API
HAL_StatusTypeDef BH1750_Init(BH1750_HandleTypeDef *bh, I2C_HandleTypeDef *hi2c, uint8_t addr);
HAL_StatusTypeDef BH1750_SetMode(BH1750_HandleTypeDef *bh, BH1750_Mode mode);
HAL_StatusTypeDef BH1750_ReadLux(BH1750_HandleTypeDef *bh, float *lux);

#endif /* INC_BH1750_H_ */
