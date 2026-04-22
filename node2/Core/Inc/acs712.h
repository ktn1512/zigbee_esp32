/*
 * asc712.h
 *
 *  Created on: 18 thg 4, 2026
 *      Author: khanh
 */

#ifndef INC_ACS712_H_
#define INC_ACS712_H_

#include "main.h"

// ===== CONFIG =====
#define ACS712_ADC_MAX     4095.0f
#define ACS712_VREF        3.3f

// chọn loại cảm biến
#define ACS712_SENSITIVITY 0.185f   // 5A: 0.185 | 20A: 0.100 | 30A: 0.066

#define ACS712_SAMPLE      1000
#define ACS712_NOISE       0.15f

typedef struct {
	ADC_HandleTypeDef *hadc;
	float offset;
	float last_filtered;
} ACS712_HandleTypeDef;

// ===== API =====
void ACS712_Init(ACS712_HandleTypeDef *acs, ADC_HandleTypeDef *hadc);
void ACS712_Calibrate(ACS712_HandleTypeDef *acs);
float ACS712_ReadCurrent(ACS712_HandleTypeDef *acs);
float ACS712_CalcPower(float current);

#endif /* INC_ACS712_H_ */
