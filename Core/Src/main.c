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
#include "PID.h"
#include "stm32f103xb.h"
#include "stm32f1xx_hal_uart.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define ENCODER_COUNTS_PER_REV 2048.0f
#define UART_PUBLISH_INTERVAL_MS 20U

/* Motor control constants */
#define MOTOR_PWM_DUTY_MAX  3199U

/* PID tuning — gains are for normalized output [0, 1]; firmware scales to counts after compute
 * Derived from identified plant: G(s) = 131.15 / (1 + 0.10094*s), input = normalized PWM */
#define PID_SAMPLE_TIME_S   0.020f
#define PID_KP              0.004f
#define PID_KI              0.040f
#define PID_KD              0.000f
#define PID_TAU             0.020f

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
char uart_buffer[64];
uint32_t last_uart_publish_tick = 0;

static uint8_t experiment_running = 0;
static float rpm_setpoint = 0.0f;
static char rx_buf[8];
static uint8_t rx_idx = 0;
PIDController motor_pid;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static float encoder_compute_rpm(uint16_t current_count,
                                 uint16_t previous_count,
                                 uint32_t elapsed_ms)
{
  if (elapsed_ms == 0U)
  {
    return 0.0f;
  }

  int16_t count_delta = (int16_t)(current_count - previous_count);
  float elapsed_minutes = ((float)elapsed_ms) / 60000.0f;

  return ((float)count_delta / ENCODER_COUNTS_PER_REV) / elapsed_minutes;
}

/**
  * @brief Set motor to forward direction and apply PWM duty
  * @param duty: PWM duty cycle (0 to 3199)
  */
static void motor_set_forward(uint16_t duty)
{
  HAL_GPIO_WritePin(DO_IN3_GPIO_Port, DO_IN3_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(DO_IN4_GPIO_Port, DO_IN4_Pin, GPIO_PIN_SET);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, duty);
}

/**
  * @brief Set motor to reverse direction and apply PWM duty
  * @param duty: PWM duty cycle (0 to 3199)
  */
static void motor_set_reverse(uint16_t duty)
{
  HAL_GPIO_WritePin(DO_IN4_GPIO_Port, DO_IN4_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(DO_IN3_GPIO_Port, DO_IN3_Pin, GPIO_PIN_SET);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, duty);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

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
  MX_TIM3_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  uint16_t counter_value = 0;
  uint16_t past_counter_value = 0;
  float motor_rpm = 0.0f;
  HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
  __HAL_TIM_SET_COUNTER(&htim3, 0U);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

  motor_pid.Kp = PID_KP;
  motor_pid.Ki = PID_KI;
  motor_pid.Kd = PID_KD;
  motor_pid.tau = PID_TAU;
  motor_pid.T = PID_SAMPLE_TIME_S;
  motor_pid.limMin = -1.0f;
  motor_pid.limMax = 1.0f;
  motor_pid.limMinInt = -1.0f;
  motor_pid.limMaxInt = 1.0f;
  PID_Init(&motor_pid);
  /* Send 's' to arm, then any integer RPM value (e.g. "120") to set the target */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t current_tick = HAL_GetTick();

    /* UART RX: buffer chars until newline, then parse command.
     * 's'       — arm the experiment (motor off, setpoint = 0)
     * '0'..'9'  — update RPM setpoint forward (e.g. "120\n")
     * '-' + digits — set negative RPM setpoint for reverse (e.g. "-120\n")
     */
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE))
    {
      uint8_t rx_byte = (uint8_t)(huart2.Instance->DR & 0xFFU);
      if (rx_byte == '\r' || rx_byte == '\n')
      {
        if (rx_idx > 0U)
        {
          rx_buf[rx_idx] = '\0';
          if (rx_idx == 1U && rx_buf[0] == 's')
          {
            experiment_running = 1;
            last_uart_publish_tick = current_tick;
            past_counter_value = __HAL_TIM_GET_COUNTER(&htim3);
            PID_Init(&motor_pid);
            rpm_setpoint = 0.0f;
          }
          else
          {
            int32_t val = 0;
            uint8_t start = 0U;
            int8_t sign = 1;
            if (rx_buf[0] == '-')
            {
              sign = -1;
              start = 1U;
            }
            for (uint8_t i = start; i < rx_idx; i++)
            {
              if (rx_buf[i] >= '0' && rx_buf[i] <= '9')
              {
                val = val * 10 + (int32_t)(rx_buf[i] - '0');
              }
            }
            rpm_setpoint = (float)(sign * val);
          }
          rx_idx = 0U;
        }
      }
      else if (rx_idx < (uint8_t)(sizeof(rx_buf) - 1U))
      {
        rx_buf[rx_idx++] = (char)rx_byte;
      }
    }

    if (experiment_running)
    {
      counter_value = __HAL_TIM_GET_COUNTER(&htim3);

      if ((current_tick - last_uart_publish_tick) >= UART_PUBLISH_INTERVAL_MS)
      {
        uint32_t sample_elapsed_ms = current_tick - last_uart_publish_tick;

        motor_rpm = encoder_compute_rpm(counter_value, past_counter_value, sample_elapsed_ms);

        /* PID velocity control — normalized output then scale to counts */
        float u_norm = PID_Compute(&motor_pid, rpm_setpoint, motor_rpm);
        float u_abs  = (u_norm < 0.0f) ? -u_norm : u_norm;
        uint16_t pwm_command = (uint16_t)(u_abs * (float)MOTOR_PWM_DUTY_MAX);

        if (rpm_setpoint == 0.0f)
        {
          HAL_GPIO_WritePin(DO_IN3_GPIO_Port, DO_IN3_Pin, GPIO_PIN_RESET);
          HAL_GPIO_WritePin(DO_IN4_GPIO_Port, DO_IN4_Pin, GPIO_PIN_RESET);
          __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0U);
        }
        else if (u_norm >= 0.0f)
        {
          motor_set_forward(pwm_command);
        }
        else
        {
          motor_set_reverse(pwm_command);
        }

        /* Telemetry — log current-cycle command alongside current measurement */
        const char *u_sign = (u_norm < 0.0f) ? "-" : "";
        uint32_t u_int  = (uint32_t)u_abs;
        uint32_t u_frac = (uint32_t)((u_abs - (float)u_int) * 1000.0f);

        int tx_len = snprintf(uart_buffer,
                              sizeof(uart_buffer),
                              "%d,%d,%s%lu.%03lu\r\n",
                              (int)motor_rpm,
                              (int)rpm_setpoint,
                              u_sign,
                              (unsigned long)u_int,
                              (unsigned long)u_frac);

        if (tx_len > 0)
        {
          HAL_UART_Transmit(&huart2, (uint8_t*)uart_buffer, (uint16_t)tx_len, 20);
        }
        last_uart_publish_tick = current_tick;
        past_counter_value = counter_value;
      }
    }

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 3199;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 5;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LD2_Pin|DO_IN3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DO_IN4_GPIO_Port, DO_IN4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin DO_IN3_Pin */
  GPIO_InitStruct.Pin = LD2_Pin|DO_IN3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : DO_IN4_Pin */
  GPIO_InitStruct.Pin = DO_IN4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DO_IN4_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
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
