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
#include "fatfs.h"
#include "user_diskio.h"
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
volatile DSTATUS sd_status;
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/

/* Button state */
#define BUTTON_RELEASED                    0U
#define BUTTON_PRESSED                     1U
/* USER CODE BEGIN PM */
#define AUDIO_READ_SIZE 4096
#define DAC_BUFFER_SIZE 1024

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

__IO uint32_t BspButtonState = BUTTON_RELEASED;
DAC_HandleTypeDef hdac1;
DMA_HandleTypeDef hdma_dac_ch1;

SPI_HandleTypeDef hspi2;
SPI_HandleTypeDef hspi3;

TIM_HandleTypeDef htim6;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_DAC1_Init(void);
static void MX_SPI2_Init(void);
static void MX_SPI3_Init(void);
static void MX_TIM6_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*
static uint16_t read_u16_le(const BYTE *p)
{
    return (uint16_t)p[0] |
           ((uint16_t)p[1] << 8);
}

static uint32_t read_u32_le(const BYTE *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
*/

static void ITM_SendString(const char *str)
{
    while (*str)
    {
        ITM_SendChar(*str++);
    }
}

static void ITM_SendUint32(uint32_t value)
{
    char digits[10];
    int i = 0;

    if (value == 0)
    {
        ITM_SendChar('0');
        return;
    }

    while (value > 0)
    {
        digits[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0)
    {
        ITM_SendChar(digits[--i]);
    }
}

static void SD_SetFastSPI(void)
{
    HAL_SPI_DeInit(&hspi2);

    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;

    if (HAL_SPI_Init(&hspi2) != HAL_OK)
    {
        Error_Handler();
    }
}

typedef struct
{
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t data_size;
    uint32_t data_offset;
} WAV_Info;

static FRESULT WAV_ParseHeader(FIL *file, WAV_Info *wav)
{
    uint8_t header[44];
    UINT bytes_read;

    FRESULT result;

    result = f_read(file, header, sizeof(header), &bytes_read);

    if (result != FR_OK)
    {
        return result;
    }

    if (bytes_read != sizeof(header))
    {
        return FR_INT_ERR;
    }

    if (memcmp(&header[0], "RIFF", 4) != 0)
    {
        return FR_INT_ERR;
    }

    if (memcmp(&header[8], "WAVE", 4) != 0)
    {
        return FR_INT_ERR;
    }

    if (memcmp(&header[12], "fmt ", 4) != 0)
    {
        return FR_INT_ERR;
    }

    if (memcmp(&header[36], "data", 4) != 0)
    {
        return FR_INT_ERR;
    }

    wav->audio_format =
        header[20] |
        (header[21] << 8);

    wav->num_channels =
        header[22] |
        (header[23] << 8);

    wav->sample_rate =
        header[24] |
        (header[25] << 8) |
        (header[26] << 16) |
        (header[27] << 24);

    wav->byte_rate =
        header[28] |
        (header[29] << 8) |
        (header[30] << 16) |
        (header[31] << 24);

    wav->block_align =
        header[32] |
        (header[33] << 8);

    wav->bits_per_sample =
        header[34] |
        (header[35] << 8);

    wav->data_size =
        header[40] |
        (header[41] << 8) |
        (header[42] << 16) |
        (header[43] << 24);

    wav->data_offset = 44;

    return FR_OK;
}

uint8_t audio_read_buffer[AUDIO_READ_SIZE];
uint16_t dac_buffer[DAC_BUFFER_SIZE];

static uint32_t PCM_To_DAC(
    const uint8_t *pcm,
    uint32_t pcm_bytes,
    uint16_t *dac_buffer)
{
    uint32_t num_frames = pcm_bytes / 4;

    for (uint32_t i = 0; i < num_frames; i++)
    {
        int16_t left =
            (int16_t)(
                pcm[i * 4] |
                (pcm[i * 4 + 1] << 8)
            );

        dac_buffer[i] =
            ((int32_t)left + 32768) >> 4;
    }

    return num_frames;
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
  MX_DMA_Init();
  MX_DAC1_Init();
  MX_SPI2_Init();
  MX_SPI3_Init();
  MX_FATFS_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */

  sd_status = USER_initialize(0);

  if (sd_status == 0)
  {
      SD_SetFastSPI();
  }

  ITM_SendString("SD stage: ");
  ITM_SendUint32(sd_debug_stage);
  ITM_SendString(" response: ");
  ITM_SendUint32(sd_debug_response);
  ITM_SendChar('\n');

  ITM_SendChar('S');
  ITM_SendChar('D');
  ITM_SendChar(':');
  ITM_SendChar(' ');

  const char hex[] = "0123456789ABCDEF";

  ITM_SendChar(hex[(sd_status >> 4) & 0x0F]);
  ITM_SendChar(hex[sd_status & 0x0F]);

  ITM_SendChar('\r');
  ITM_SendChar('\n');


  FRESULT result;

  result = f_mount(&USERFatFS, USERPath, 1);

  ITM_SendChar('M');
  ITM_SendChar('O');
  ITM_SendChar('U');
  ITM_SendChar('N');
  ITM_SendChar('T');
  ITM_SendChar(':');
  ITM_SendChar(' ');

  ITM_SendChar(hex[(result >> 4) & 0x0F]);
  ITM_SendChar(hex[result & 0x0F]);

  ITM_SendChar('\r');
  ITM_SendChar('\n');

  BYTE buffer[512];
  DRESULT read_result;

  read_result = USER_read(0, buffer, 0, 1);

  ITM_SendChar('R');
  ITM_SendChar('E');
  ITM_SendChar('A');
  ITM_SendChar('D');
  ITM_SendChar(':');
  ITM_SendChar(' ');

  ITM_SendChar(hex[(read_result >> 4) & 0x0F]);
  ITM_SendChar(hex[read_result & 0x0F]);

  ITM_SendChar('\r');
  ITM_SendChar('\n');

  ITM_SendChar('S');
  ITM_SendChar('0');
  ITM_SendChar(':');
  ITM_SendChar(' ');

  for (int i = 0; i < 16; i++)
  {
      ITM_SendChar(hex[(buffer[i] >> 4) & 0x0F]);
      ITM_SendChar(hex[buffer[i] & 0x0F]);
      ITM_SendChar(' ');
  }

  ITM_SendChar('\r');
  ITM_SendChar('\n');

  ITM_SendChar('5');
  ITM_SendChar('1');
  ITM_SendChar('0');
  ITM_SendChar(':');
  ITM_SendChar(' ');

  ITM_SendChar(hex[(buffer[510] >> 4) & 0x0F]);
  ITM_SendChar(hex[buffer[510] & 0x0F]);
  ITM_SendChar(' ');

  ITM_SendChar(hex[(buffer[511] >> 4) & 0x0F]);
  ITM_SendChar(hex[buffer[511] & 0x0F]);

  ITM_SendChar('\r');
  ITM_SendChar('\n');

  ITM_SendChar('P');
  ITM_SendChar('1');
  ITM_SendChar(':');
  ITM_SendChar(' ');

  for (int i = 446; i < 462; i++)
  {
      ITM_SendChar(hex[(buffer[i] >> 4) & 0x0F]);
      ITM_SendChar(hex[buffer[i] & 0x0F]);
      ITM_SendChar(' ');
  }

  ITM_SendChar('\r');
  ITM_SendChar('\n');

  FIL file;
  UINT bytes_read2;

  result = f_open(&file, "0:/test.txt", FA_READ);

  if (result == FR_OK)
  {
      result = f_read(&file, (void *)buffer, sizeof(buffer) - 1, &bytes_read2);

      if (result == FR_OK)
      {
          buffer[bytes_read2] = '\0';

          for (UINT i = 0; i < bytes_read2; i++)
          {
              ITM_SendChar(buffer[i]);
          }

          ITM_SendChar('\r');
          ITM_SendChar('\n');
      }

      f_close(&file);
  }

  DIR dir;
  FILINFO fno;

  FRESULT dir_result;

  ITM_SendString("\r\n--- SD FILES ---\r\n");

  dir_result = f_opendir(&dir, "0:/");

  if (dir_result == FR_OK)
  {
      while (1)
      {
          dir_result = f_readdir(&dir, &fno);

          if (dir_result != FR_OK || fno.fname[0] == 0)
          {
              break;
          }

          ITM_SendString(fno.fname);
          ITM_SendString("\r\n");
      }

      f_closedir(&dir);
  }
  else
  {
      ITM_SendString("Failed to open directory\r\n");
  }

  FIL wav_file;
  ITM_SendString("\r\nOpening WAV...\r\n");

  WAV_Info wav;

  result = f_open(&wav_file, "0:/BASS1.WAV", FA_READ);

  if (result == FR_OK)
  {
      ITM_SendString("WAV opened\n");

      result = WAV_ParseHeader(&wav_file, &wav);

      if (result == FR_OK)
      {
          ITM_SendString("WAV header parsed\n");

          ITM_SendString("Sample rate: ");
          ITM_SendUint32(wav.sample_rate);
          ITM_SendString("\n");

          ITM_SendString("Channels: ");
          ITM_SendUint32(wav.num_channels);
          ITM_SendString("\n");

          ITM_SendString("Bits/sample: ");
          ITM_SendUint32(wav.bits_per_sample);
          ITM_SendString("\n");

          ITM_SendString("Data size: ");
          ITM_SendUint32(wav.data_size);
          ITM_SendString("\n");
      }
  }

  UINT bytes_read;

  result = f_read(
      &wav_file,
      audio_read_buffer,
      AUDIO_READ_SIZE,
      &bytes_read
  );
  ITM_SendString("Read bytes: ");
  ITM_SendUint32(bytes_read);
  ITM_SendString("\n");

  uint32_t dac_samples = PCM_To_DAC(
	  audio_read_buffer,
      bytes_read,
      dac_buffer
  );

  ITM_SendString("DAC samples: ");
  ITM_SendUint32(dac_samples);
  ITM_SendChar('\n');

  HAL_DAC_Start_DMA(
      &hdac1,
      DAC_CHANNEL_1,
      (uint32_t *)dac_buffer,
      DAC_BUFFER_SIZE,
      DAC_ALIGN_12B_R
  );

  HAL_TIM_Base_Start(&htim6);

  result = f_open(&wav_file, "0:/BASS1.WAV", FA_READ);

  if (result == FR_OK)
  {
      ITM_SendString("WAV opened\n");

      f_lseek(&wav_file, 44 + 20000);

      UINT bytes_read;

      result = f_read(
          &wav_file,
          audio_read_buffer,
          AUDIO_READ_SIZE,
          &bytes_read
      );

      if (result == FR_OK)
      {
          uint32_t dac_samples = PCM_To_DAC(
              audio_read_buffer,
              bytes_read,
              dac_buffer
          );

          ITM_SendString("DAC samples: ");
          ITM_SendUint32(dac_samples);
          ITM_SendChar('\n');

          HAL_DAC_Start_DMA(
              &hdac1,
              DAC_CHANNEL_1,
              (uint32_t *)dac_buffer,
              dac_samples,
              DAC_ALIGN_12B_R
          );

          HAL_TIM_Base_Start(&htim6);
      }
  }
  /*
  ITM_SendString("First 128 PCM bytes:\n");

  for (int i = 0; i < 128; i++)
  {
      ITM_SendUint32(audio_read_buffer[i]);
      ITM_SendChar(' ');
  }

  ITM_SendChar('\n');

  uint32_t total_read = 0;
  uint32_t nonzero_bytes = 0;

  while (total_read < wav.data_size)
  {
      uint32_t remaining = wav.data_size - total_read;

      UINT bytes_to_read =
          (remaining < AUDIO_READ_SIZE)
          ? remaining
          : AUDIO_READ_SIZE;

      UINT bytes_read = 0;

      FRESULT result = f_read(
          &wav_file,
          audio_read_buffer,
          bytes_to_read,
          &bytes_read
      );

      if (result != FR_OK)
      {
          ITM_SendString("WAV read error\n");
          break;
      }

      if (bytes_read == 0)
      {
          break;
      }

      for (UINT i = 0; i < bytes_read; i++)
      {
          if (audio_read_buffer[i] != 0)
          {
              nonzero_bytes++;
          }
      }

      total_read += bytes_read;
  }

  ITM_SendString("Total PCM bytes read: ");
  ITM_SendUint32(total_read);
  ITM_SendString("\n");

  ITM_SendString("Nonzero bytes: ");
  ITM_SendUint32(nonzero_bytes);
  ITM_SendString("\n");
  */



  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* USER CODE BEGIN BSP */

  /* -- Sample board code to switch on leds ---- */
  BSP_LED_On(LED_GREEN);

  /* USER CODE END BSP */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    /* -- Sample board code for User push-button in interrupt mode ---- */
    if (BspButtonState == BUTTON_PRESSED)
    {
      ITM_SendChar('P');
      /* Update button state */
      BspButtonState = BUTTON_RELEASED;
      /* -- Sample board code to toggle leds ---- */
      BSP_LED_Toggle(LED_GREEN);
      /* ..... Perform your action ..... */
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
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief DAC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC1_Init(void)
{

  /* USER CODE BEGIN DAC1_Init 0 */

  /* USER CODE END DAC1_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC1_Init 1 */

  /* USER CODE END DAC1_Init 1 */

  /** DAC Initialization
  */
  hdac1.Instance = DAC1;
  if (HAL_DAC_Init(&hdac1) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
  sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_DISABLE;
  sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC1_Init 2 */

  /* USER CODE END DAC1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_MASTER;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  hspi3.Init.DataSize = SPI_DATASIZE_4BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 7;
  hspi3.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi3.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 0;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 1813;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : SD_CS_Pin */
  GPIO_InitStruct.Pin = SD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SD_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : USART_TX_Pin USART_RX_Pin */
  GPIO_InitStruct.Pin = USART_TX_Pin|USART_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief EXTI line detection callbacks
  * @param GPIO_Pin: Specifies the pins connected EXTI line
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin==USER_BUTTON_PIN)
  {
    BspButtonState = BUTTON_PRESSED;
  }
}

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
