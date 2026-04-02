/*
 * dht11.h
 *
 *  Created on: 25 thg 3, 2026
 *      Author: khanh
 */

#ifndef INC_DHT11_H_
#define INC_DHT11_H_

#include "stm32f1xx_hal.h"

// Struct
typedef struct {
	GPIO_TypeDef *port;
	uint16_t pin;
	TIM_HandleTypeDef *htim;   // 👈 thêm timer
} DHT11_HandleTypeDef;

// API
void DHT11_Init(DHT11_HandleTypeDef *dht, GPIO_TypeDef *port, uint16_t pin,
		TIM_HandleTypeDef *htim);

HAL_StatusTypeDef DHT11_Read(DHT11_HandleTypeDef *dht, float *temperature,
		float *humidity);

#endif /* INC_DHT11_H_ */
