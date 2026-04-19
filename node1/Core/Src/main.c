/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "string.h"
#include "stdbool.h"
#include "dht11.h"
#include "bh1750.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define RX_BUF_SIZE 64
#define NODE_ID "NODE1"   // đổi NODE2 cho board còn lại

#define LED1_PORT GPIOA
#define LED1_PIN GPIO_PIN_3

#define LED2_PORT GPIOA
#define LED2_PIN GPIO_PIN_5

#define FAN1_PORT GPIOC
#define FAN1_PIN  GPIO_PIN_13

#define FAN2_PORT GPIOC
#define FAN2_PIN  GPIO_PIN_14

#define TEMP_ON  30.0
#define TEMP_OFF 26.0

uint8_t rx_buf[RX_BUF_SIZE];
uint8_t rx_char;

volatile uint16_t rx_len = 0;
volatile uint8_t rx_flag = 0;

BH1750_HandleTypeDef bh1, bh2;
DHT11_HandleTypeDef dht1, dht2;

float temp1, hum1, lux1;
float temp2, hum2, lux2;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM4_Init(void);
/* USER CODE BEGIN PFP */
void UART_Start_DMA(void);
void Send_Data(void);
void Process_Data(void);

void Send_Data(void) {
	char msg[100];

	if (DHT11_Read(&dht1, &temp1, &hum1) != HAL_OK) {
		sprintf(msg, "%s:1:DHT_ERROR\r\n", NODE_ID);
		HAL_UART_Transmit(&huart1, (uint8_t*) msg, strlen(msg), 100);
		return;
	}

	if (BH1750_ReadLux(&bh1, &lux1) != HAL_OK) {
		sprintf(msg, "%s:1:BH1750_ERROR\r\n", NODE_ID);
		HAL_UART_Transmit(&huart1, (uint8_t*) msg, strlen(msg), 100);
		return;
	}

	sprintf(msg, "%s:1:TEMP:%.1f\r\n", NODE_ID, temp1);
	HAL_UART_Transmit(&huart1, (uint8_t*) msg, strlen(msg), 100);

	sprintf(msg, "%s:1:HUMID:%.1f\r\n", NODE_ID, hum1);
	HAL_UART_Transmit(&huart1, (uint8_t*) msg, strlen(msg), 100);

	sprintf(msg, "%s:1:LUX:%.1f\r\n", NODE_ID, lux1);
	HAL_UART_Transmit(&huart1, (uint8_t*) msg, strlen(msg), 100);

	if (DHT11_Read(&dht2, &temp2, &hum2) != HAL_OK) {
		sprintf(msg, "%s:2:DHT_ERROR\r\n", NODE_ID);
		HAL_UART_Transmit(&huart1, (uint8_t*) msg, strlen(msg), 100);
		return;
	}

	if (BH1750_ReadLux(&bh2, &lux2) != HAL_OK) {
		sprintf(msg, "%s:2:BH1750_ERROR\r\n", NODE_ID);
		HAL_UART_Transmit(&huart1, (uint8_t*) msg, strlen(msg), 100);
		return;
	}

	sprintf(msg, "%s:2:TEMP:%.1f\r\n", NODE_ID, temp2);
	HAL_UART_Transmit(&huart1, (uint8_t*) msg, strlen(msg), 100);

	sprintf(msg, "%s:2:HUMID:%.1f\r\n", NODE_ID, hum2);
	HAL_UART_Transmit(&huart1, (uint8_t*) msg, strlen(msg), 100);

	sprintf(msg, "%s:2:LUX:%.1f\r\n", NODE_ID, lux2);
	HAL_UART_Transmit(&huart1, (uint8_t*) msg, strlen(msg), 100);
}

void Process_Data(void) {

	uint16_t n = rx_len;
	if (n >= RX_BUF_SIZE)
		n = RX_BUF_SIZE - 1;

	rx_buf[n] = '\0';

	char *saveptr_line;
	char *line = strtok_r((char*) rx_buf, "\r\n", &saveptr_line);

	while (line != NULL) {

		char buf[64];
		strncpy(buf, line, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';

		char *saveptr_token;

		char *type = strtok_r(buf, ":", &saveptr_token);

		// ===== CHECK FORMAT =====
		if (type && strcmp(type, "CMD") == 0) {

			char *id = strtok_r(NULL, ":", &saveptr_token);
			char *node = strtok_r(NULL, ":", &saveptr_token);
			char *group = strtok_r(NULL, ":", &saveptr_token);
			char *cmd = strtok_r(NULL, ":", &saveptr_token);
			char *value = strtok_r(NULL, ":", &saveptr_token);

			if (id && node && group && cmd && value) {

				// ===== HANDLE COMMAND =====
				if (strcmp(node, NODE_ID) == 0 || strcmp(node, "ALL") == 0) {

					if (strcmp(group, "1") == 0) {

						if (strcmp(cmd, "LED") == 0) {
							HAL_GPIO_WritePin(LED1_PORT, LED1_PIN,
									strcmp(value, "ON") == 0 ?
											GPIO_PIN_RESET : GPIO_PIN_SET);
						} else if (strcmp(cmd, "FAN") == 0) {
							HAL_GPIO_WritePin(FAN1_PORT, FAN1_PIN,
									strcmp(value, "ON") == 0 ?
											GPIO_PIN_RESET : GPIO_PIN_SET);
						}
					}

					else if (strcmp(group, "2") == 0) {

						if (strcmp(cmd, "LED") == 0) {
							HAL_GPIO_WritePin(LED2_PORT, LED2_PIN,
									strcmp(value, "ON") == 0 ?
											GPIO_PIN_RESET : GPIO_PIN_SET);
						} else if (strcmp(cmd, "FAN") == 0) {
							HAL_GPIO_WritePin(FAN2_PORT, FAN2_PIN,
									strcmp(value, "ON") == 0 ?
											GPIO_PIN_RESET : GPIO_PIN_SET);
						}
					}
				}

				// ===== SEND ACK =====
				char ack[32];
				sprintf(ack, "ACK:%s:OK\r\n", id);
				HAL_UART_Transmit(&huart1, (uint8_t*) ack, strlen(ack), 100);
			}
		}

		// ===== NEXT LINE =====
		line = strtok_r(NULL, "\r\n", &saveptr_line);
	}
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void UART_Start_DMA(void) {
	memset(rx_buf, 0, RX_BUF_SIZE);

	HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buf, RX_BUF_SIZE);

	if (huart1.hdmarx) {
		__HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
	}
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_USART1_UART_Init();
	MX_I2C1_Init();
	MX_TIM4_Init();
	/* USER CODE BEGIN 2 */
	HAL_TIM_Base_Start(&htim4);

// init sensor
	BH1750_Init(&bh1, &hi2c1, BH1750_ADDR_LOW);
	DHT11_Init(&dht1, GPIOB, GPIO_PIN_14, &htim4);
	BH1750_Init(&bh2, &hi2c1, BH1750_ADDR_HIGH);
	DHT11_Init(&dht2, GPIOB, GPIO_PIN_15, &htim4);

// start UART DMA
	UART_Start_DMA();
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	uint32_t last_time = 0;

	while (1) {
		// gửi mỗi 2s
		if (HAL_GetTick() - last_time > 2000) {
			last_time = HAL_GetTick();
			Send_Data();
		}

		// xử lý nhận từ CC2530
		if (rx_flag) {
			rx_flag = 0;
			Process_Data();
			UART_Start_DMA();
		}
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

	/* USER CODE BEGIN I2C1_Init 0 */

	/* USER CODE END I2C1_Init 0 */

	/* USER CODE BEGIN I2C1_Init 1 */

	/* USER CODE END I2C1_Init 1 */
	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 100000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief TIM4 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM4_Init(void) {

	/* USER CODE BEGIN TIM4_Init 0 */

	/* USER CODE END TIM4_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM4_Init 1 */

	/* USER CODE END TIM4_Init 1 */
	htim4.Instance = TIM4;
	htim4.Init.Prescaler = 71;
	htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim4.Init.Period = 65535;
	htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim4) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM4_Init 2 */

	/* USER CODE END TIM4_Init 2 */

}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

	/* USER CODE BEGIN USART1_Init 0 */

	/* USER CODE END USART1_Init 0 */

	/* USER CODE BEGIN USART1_Init 1 */

	/* USER CODE END USART1_Init 1 */
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */

}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void) {

	/* DMA controller clock enable */
	__HAL_RCC_DMA1_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA1_Channel5_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13 | GPIO_PIN_14, GPIO_PIN_SET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 | GPIO_PIN_5, GPIO_PIN_SET);

	/*Configure GPIO pins : PC13 PC14 */
	GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : PA3 PA5 */
	GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
	if (huart->Instance == USART1) {
		if (Size > 2)   // ❗ QUAN TRỌNG (lọc nhiễu)
				{
			rx_len = Size;
			rx_flag = 1;
		} else {
			// nhận lại luôn, bỏ qua
			UART_Start_DMA();
		}
	}
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
