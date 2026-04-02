/*
 * bh1750.c
 *
 *  Created on: 24 thg 3, 2026
 *      Author: khanh
 */
#include "bh1750.h"

#define BH1750_POWER_ON  0x01
#define BH1750_RESET     0x07

// Delay theo mode (ms)
static uint16_t BH1750_GetDelay(BH1750_Mode mode) {
	switch (mode) {
	case BH1750_CONT_LORES:
	case BH1750_ONE_LORES:
		return 20;
	default:
		return 180;
	}
}

// Gửi command
static HAL_StatusTypeDef BH1750_WriteCmd(BH1750_HandleTypeDef *bh, uint8_t cmd) {
	return HAL_I2C_Master_Transmit(bh->hi2c, bh->address, &cmd, 1, 100);
}

// Init
HAL_StatusTypeDef BH1750_Init(BH1750_HandleTypeDef *bh, I2C_HandleTypeDef *hi2c,
		uint8_t addr) {
	bh->hi2c = hi2c;
	bh->address = addr;
	bh->mode = BH1750_CONT_HIRES;

	// Power ON
	if (BH1750_WriteCmd(bh, BH1750_POWER_ON) != HAL_OK)
		return HAL_ERROR;

	HAL_Delay(10);

	// Reset
	if (BH1750_WriteCmd(bh, BH1750_RESET) != HAL_OK)
		return HAL_ERROR;

	HAL_Delay(10);

	return BH1750_SetMode(bh, bh->mode);
}

// Set mode
HAL_StatusTypeDef BH1750_SetMode(BH1750_HandleTypeDef *bh, BH1750_Mode mode) {
	bh->mode = mode;
	return BH1750_WriteCmd(bh, mode);
}

// Read lux (FIX CHÍNH Ở ĐÂY)
HAL_StatusTypeDef BH1750_ReadLux(BH1750_HandleTypeDef *bh, float *lux) {
	uint8_t data[2];

	//LUÔN trigger đo lại
	if (BH1750_WriteCmd(bh, bh->mode) != HAL_OK)
		return HAL_ERROR;

	HAL_Delay(BH1750_GetDelay(bh->mode));

	// Read 2 bytes
	if (HAL_I2C_Master_Receive(bh->hi2c, bh->address, data, 2, 100) != HAL_OK)
		return HAL_ERROR;

	uint16_t raw = (data[0] << 8) | data[1];

	if (raw == 0xFFFF)
		return HAL_ERROR; // lỗi bus

	// Convert lux
	float factor = 1.2f;

	if (bh->mode == BH1750_CONT_HIRES2 || bh->mode == BH1750_ONE_HIRES2)
		factor = 2.4f;

	*lux = raw / factor;

	return HAL_OK;
}
