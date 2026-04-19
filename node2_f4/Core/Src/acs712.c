/*
 * asc712.c
 *
 *  Created on: 18 thg 4, 2026
 *      Author: khanh
 */

#include "acs712.h"
#include <math.h>

// ===== INTERNAL ADC READ =====
static uint16_t ACS712_ReadADC(ACS712_HandleTypeDef *acs) {
    HAL_ADC_Start(acs->hadc);
    HAL_ADC_PollForConversion(acs->hadc, HAL_MAX_DELAY);
    uint16_t val = HAL_ADC_GetValue(acs->hadc);
    HAL_ADC_Stop(acs->hadc);
    return val;
}

// ===== INIT =====
void ACS712_Init(ACS712_HandleTypeDef *acs, ADC_HandleTypeDef *hadc) {
    acs->hadc = hadc;
    acs->offset = 0;
    acs->last_filtered = 0;
}

// ===== CALIB =====
void ACS712_Calibrate(ACS712_HandleTypeDef *acs) {
    float sum = 0;

    for (int i = 0; i < 2000; i++) {
        uint16_t adc = ACS712_ReadADC(acs);
        float voltage = adc * ACS712_VREF / ACS712_ADC_MAX;
        sum += voltage;
    }

    acs->offset = sum / 2000.0f;
}

// ===== READ RMS CURRENT =====
float ACS712_ReadCurrent(ACS712_HandleTypeDef *acs) {
    float sum = 0;
    float sum_sq = 0;

    // ===== lấy mean =====
    for (int i = 0; i < ACS712_SAMPLE; i++) {
        uint16_t adc = ACS712_ReadADC(acs);
        float voltage = adc * ACS712_VREF / ACS712_ADC_MAX;
        sum += voltage;
    }

    float mean = sum / ACS712_SAMPLE;

    // ===== tính RMS =====
    for (int i = 0; i < ACS712_SAMPLE; i++) {
        uint16_t adc = ACS712_ReadADC(acs);
        float voltage = adc * ACS712_VREF / ACS712_ADC_MAX;

        float current = (voltage - mean) / ACS712_SENSITIVITY;
        sum_sq += current * current;
    }

    float irms = sqrt(sum_sq / ACS712_SAMPLE);

    // ===== lọc nhiễu =====
    if (irms < ACS712_NOISE)
        irms = 0;

    // ===== low pass filter =====
    float alpha = 0.2f;
    irms = alpha * irms + (1 - alpha) * acs->last_filtered;
    acs->last_filtered = irms;

    return irms;
}

// ===== POWER =====
float ACS712_CalcPower(float current) {
    return 220.0f * current * 0.8f;
}
