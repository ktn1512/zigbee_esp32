/*
 * dht11.c
 *
 *  Created on: 25 thg 3, 2026
 *      Author: khanh
 */
#include "dht11.h"

// ===== Delay us dùng timer bất kỳ =====
static void delay_us(DHT11_HandleTypeDef *dht, uint16_t us) {
	__HAL_TIM_SET_COUNTER(dht->htim, 0);
	while (__HAL_TIM_GET_COUNTER(dht->htim) < us)
		;
}

// ===== Set pin output =====
static void DHT11_SetOutput(DHT11_HandleTypeDef *dht) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	GPIO_InitStruct.Pin = dht->pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(dht->port, &GPIO_InitStruct);
}

// ===== Set pin input =====
static void DHT11_SetInput(DHT11_HandleTypeDef *dht) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	GPIO_InitStruct.Pin = dht->pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(dht->port, &GPIO_InitStruct);
}

// ===== INIT =====
void DHT11_Init(DHT11_HandleTypeDef *dht, GPIO_TypeDef *port, uint16_t pin,
		TIM_HandleTypeDef *htim) {
	dht->port = port;
	dht->pin = pin;
	dht->htim = htim;
}

// ===== READ =====
HAL_StatusTypeDef DHT11_Read(DHT11_HandleTypeDef *dht, float *temperature,
		float *humidity) {
	uint8_t data[5] = { 0 };
	uint32_t timeout;

	// ===== START SIGNAL =====
	DHT11_SetOutput(dht);
	HAL_GPIO_WritePin(dht->port, dht->pin, GPIO_PIN_RESET);
	HAL_Delay(20);

	HAL_GPIO_WritePin(dht->port, dht->pin, GPIO_PIN_SET);
	delay_us(dht, 30);

	DHT11_SetInput(dht);

	// ===== RESPONSE =====
	timeout = 0;
	while (HAL_GPIO_ReadPin(dht->port, dht->pin))
		if (++timeout > 10000)
			return HAL_TIMEOUT;

	timeout = 0;
	while (!HAL_GPIO_ReadPin(dht->port, dht->pin))
		if (++timeout > 10000)
			return HAL_TIMEOUT;

	timeout = 0;
	while (HAL_GPIO_ReadPin(dht->port, dht->pin))
		if (++timeout > 10000)
			return HAL_TIMEOUT;

	// ===== READ 40 BIT =====
	for (int i = 0; i < 40; i++) {
		timeout = 0;
		while (!HAL_GPIO_ReadPin(dht->port, dht->pin))
			if (++timeout > 10000)
				return HAL_TIMEOUT;

		delay_us(dht, 40);

		if (HAL_GPIO_ReadPin(dht->port, dht->pin))
			data[i / 8] |= (1 << (7 - (i % 8)));

		timeout = 0;
		while (HAL_GPIO_ReadPin(dht->port, dht->pin))
			if (++timeout > 10000)
				return HAL_TIMEOUT;
	}

	// ===== CHECKSUM =====
	if ((data[0] + data[1] + data[2] + data[3]) != data[4])
		return HAL_ERROR;

	*humidity = data[0];
	*temperature = data[2];

	return HAL_OK;
}
