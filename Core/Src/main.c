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
/* BLE_TFT: our display, LVGL, button input, sensors and screens */
#include "tft.h"
#include "lvgl.h"
#include "lvgl_port.h"
#include "input_port.h"
#include "sensors.h"
#include "ui.h"
#include "ble_image_service.h"   /* step 6: pause LVGL while a picture arrives */
#include "ams_client.h"          /* step 7: now playing from the iPhone */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc4;

CRC_HandleTypeDef hcrc;

RAMCFG_HandleTypeDef hramcfg_SRAM1;

RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef handle_LPDMA1_Channel0;
DMA_HandleTypeDef handle_LPDMA1_Channel1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void SystemPower_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* ===================== TFT WIRING TEST =====================================
 * Set TFT_PIN_TEST to 1, build, flash, open Tera Term (COM462, 115200).
 * 1. SHORT TEST (automatic): each signal is driven HIGH while all others are
 *    inputs with pull-down. If another one reads HIGH, the two are shorted.
 * 2. WALK TEST (you measure): one signal at a time is 3.3 V for 6 s, all
 *    others 0 V. Measure with the multimeter (DC volts) between GND and the
 *    named pin ON THE TFT SHIELD. 3.3 V = wire OK, 0 V = wire broken / wrong pin.
 *    Green LED LD2 blinks N times before signal number N.
 * The display is NOT started in this mode. Set back to 0 when done.
 *
 * TFT_PIN_TEST 2 = COLOUR TEST: no Bluetooth, no LVGL. Starts the display and
 * fills the whole screen RED, GREEN, BLUE, BLACK, WHITE (1 s each), forever.
 * Green LED LD2 toggles with every colour so you can see the program runs.
 * Colours on screen = display + wiring OK (problem is in the app).
 * Still white = the display does not accept the start-up commands.
 *
 * TFT_PIN_TEST 3 = ASK THE DISPLAY WHO IT IS (read test). Needs ONE wire moved:
 * TFT LCD_RD from IOREF (CN5-2) to CN4-36 (PA1 = button B1; don't press B1).
 * Every 2 s the display's ID and power status are read back and printed
 * (Tera Term COM462, 115200). Put RD back on IOREF afterwards.
 * Note: UART TX is PA6 = TFT D7, so the log pauses during the D7 step.
 * ========================================================================== */
#define TFT_PIN_TEST 0

#if (TFT_PIN_TEST == 1)
#include <stdio.h>
#include <string.h>
5onst char *con; } tpin_t;
static const tpin_t TP[] = {
  { GPIOA, GPIO_PIN_9,  "D0", "PA9",  "CN6-4"  },
  { GPIOA, GPIO_PIN_10, "D1", "PA10", "CN6-5"  },
  { GPIOA, GPIO_PIN_8,  "D2", "PA8",  "CN6-6"  },
  { GPIOA, GPIO_PIN_11, "D3", "PA11", "CN6-9"  },
  { GPIOB, GPIO_PIN_2,  "D4", "PB2",  "CN6-10" },
  { GPIOA, GPIO_PIN_7,  "D5", "PA7",  "CN4-6"  },
  { GPIOB, GPIO_PIN_15, "D6", "PB15", "CN4-38" },
  { GPIOA, GPIO_PIN_6,  "D7", "PA6",  "CN3-37" },
  { GPIOA, GPIO_PIN_5,  "WR", "PA5",  "CN6-3"  },
  { GPIOC, GPIO_PIN_13, "RS", "PC13", "CN4-30" },
};
#define TP_N  (sizeof TP / sizeof TP[0])
#define LD2_PORT GPIOB
#define LD2_PIN  GPIO_PIN_12

static void tp_print(const char *s)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 200);
}

static void tp_mode(int i, uint32_t mode, uint32_t pull)
{
  GPIO_InitTypeDef g = {0};
  g.Pin = TP[i].pin; g.Mode = mode; g.Pull = pull; g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TP[i].port, &g);
}

static void uart_pin_back(void)          /* PA6 back to UART TX after using it as D7 */
{
  HAL_UART_MspInit(&huart1);
}

static void tft_pin_test(void)
{
  char b[160];
  __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
  GPIO_InitTypeDef g = {0};
  g.Pin = LD2_PIN; g.Mode = GPIO_MODE_OUTPUT_PP; g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_PORT, &g);
  MX_USART1_UART_Init();

  for (;;)
  {
    /* ---------- 1. automatic tests ---------- */
    char line[TP_N][48];
    uint8_t stuck[TP_N], pulled[TP_N];
    int problems = 0;

    /* a) STUCK: drive the pin LOW for a few microseconds and read it back.
     *    Still HIGH = wire is connected straight to 3.3 V / 5 V.          */
    for (unsigned i = 0; i < TP_N; i++)
    {
      for (unsigned j = 0; j < TP_N; j++) tp_mode(j, GPIO_MODE_INPUT, GPIO_NOPULL);
      HAL_GPIO_WritePin(TP[i].port, TP[i].pin, GPIO_PIN_RESET);
      tp_mode(i, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);
      for (volatile int d = 0; d < 20; d++) { }
      stuck[i] = (HAL_GPIO_ReadPin(TP[i].port, TP[i].pin) == GPIO_PIN_SET);
      tp_mode(i, GPIO_MODE_INPUT, GPIO_NOPULL);
    }

    /* b) PULLED: all pins inputs with the chip's weak pull-down.
     *    HIGH = something on the shield pulls this line up (resistor or power). */
    for (unsigned j = 0; j < TP_N; j++) tp_mode(j, GPIO_MODE_INPUT, GPIO_PULLDOWN);
    HAL_Delay(5);
    for (unsigned j = 0; j < TP_N; j++)
      pulled[j] = (HAL_GPIO_ReadPin(TP[j].port, TP[j].pin) == GPIO_PIN_SET);

    /* c) SHORT between two signals: inputs with pull-UP, drive one LOW.
     *    Another one going LOW too = the two wires touch.                  */
    for (unsigned i = 0; i < TP_N; i++)
    {
      line[i][0] = 0;
      if (stuck[i]) continue;
      for (unsigned j = 0; j < TP_N; j++) tp_mode(j, GPIO_MODE_INPUT, GPIO_PULLUP);
      HAL_GPIO_WritePin(TP[i].port, TP[i].pin, GPIO_PIN_RESET);
      tp_mode(i, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);
      HAL_Delay(5);
      for (unsigned j = 0; j < TP_N; j++)
      {
        if (j == i || stuck[j]) continue;
        if (HAL_GPIO_ReadPin(TP[j].port, TP[j].pin) == GPIO_PIN_RESET)
        {
          size_t l = strlen(line[i]);
          snprintf(line[i] + l, sizeof line[i] - l, " %s", TP[j].tft);
        }
      }
    }
    for (unsigned j = 0; j < TP_N; j++) tp_mode(j, GPIO_MODE_INPUT, GPIO_PULLDOWN);
    uart_pin_back();

    tp_print("\r\n\r\n========== TFT WIRING TEST v2 ==========\r\n");
    for (unsigned i = 0; i < TP_N; i++)
    {
      const char *verdict;
      if (stuck[i])          { verdict = "BAD  - stuck to 3.3V/5V (wire on a power pin?)"; problems++; }
      else if (line[i][0])   { verdict = "BAD  - touching:"; problems++; }
      else if (pulled[i])    { verdict = "ok   (pulled up by the shield - normal)"; }
      else                   { verdict = "ok"; }
      snprintf(b, sizeof b, "   %-2s  board %-4s %-6s : %s%s\r\n",
               TP[i].tft, TP[i].mcu, TP[i].con, verdict, line[i]);
      tp_print(b);
    }
    tp_print(problems ? "   >>> PROBLEM FOUND - fix the BAD wires above <<<\r\n"
                      : "   >>> all 10 wires: no shorts, nothing stuck <<<\r\n"
                        "   (if the screen is still white: a wire may be BROKEN - use the walk test)\r\n");

    /* ---------- 2. walk test ---------- */
    tp_print("\r\n2) WALK TEST: measure DC volts, black probe on GND,\r\n"
             "   red probe on the named pin ON THE TFT SHIELD.\r\n"
             "   That pin = 3.3 V, all others = 0 V. 6 s each.\r\n");
    for (unsigned i = 0; i < TP_N; i++)
    {
      snprintf(b, sizeof b, "\r\n   [%u/10] TFT %-2s  <- board %-4s (%s)  should be 3.3 V now%s\r\n",
               i + 1, TP[i].tft, TP[i].mcu, TP[i].con,
               (TP[i].pin == GPIO_PIN_6 && TP[i].port == GPIOA) ? "  (log pauses)" : "");
      tp_print(b);
      HAL_Delay(20);
      for (unsigned k = 0; k <= i; k++)        /* LD2 blinks i+1 times */
      {
        HAL_GPIO_WritePin(LD2_PORT, LD2_PIN, GPIO_PIN_SET);   HAL_Delay(150);
        HAL_GPIO_WritePin(LD2_PORT, LD2_PIN, GPIO_PIN_RESET); HAL_Delay(250);
      }
      for (unsigned j = 0; j < TP_N; j++)
      {
        tp_mode(j, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);
        HAL_GPIO_WritePin(TP[j].port, TP[j].pin, (j == i) ? GPIO_PIN_SET : GPIO_PIN_RESET);
      }
      HAL_GPIO_WritePin(LD2_PORT, LD2_PIN, GPIO_PIN_SET);      /* LD2 on while measuring */
      HAL_Delay(6000);
      HAL_GPIO_WritePin(LD2_PORT, LD2_PIN, GPIO_PIN_RESET);
      for (unsigned j = 0; j < TP_N; j++) tp_mode(j, GPIO_MODE_INPUT, GPIO_PULLDOWN);
      uart_pin_back();
    }
    tp_print("\r\n   walk test done - starting again in 5 s\r\n");
    HAL_Delay(5000);
  }
}
#endif /* TFT_PIN_TEST == 1 */

#if (TFT_PIN_TEST == 3)
#include <stdio.h>
#include <string.h>
typedef struct { GPIO_TypeDef *port; uint16_t pin; } bpin_t;
static const bpin_t DB[8] = {
  { GPIOA, GPIO_PIN_9 }, { GPIOA, GPIO_PIN_10 }, { GPIOA, GPIO_PIN_8 }, { GPIOA, GPIO_PIN_11 },
  { GPIOB, GPIO_PIN_2 }, { GPIOA, GPIO_PIN_7 },  { GPIOB, GPIO_PIN_15 }, { GPIOA, GPIO_PIN_6 } };
#define RD_PORT GPIOA
#define RD_PIN  GPIO_PIN_1      /* LCD_RD moved to CN4-36 for this test */
#define WR_P    GPIOA, GPIO_PIN_5
#define RS_P    GPIOC, GPIO_PIN_13

static void us_wait(void) { for (volatile int d = 0; d < 30; d++) { } }

static void bus_mode(uint32_t mode)
{
  GPIO_InitTypeDef g = {0};
  g.Mode = mode; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
  for (int i = 0; i < 8; i++) { g.Pin = DB[i].pin; HAL_GPIO_Init(DB[i].port, &g); }
}

static void w8(int rs, uint8_t v)
{
  bus_mode(GPIO_MODE_OUTPUT_PP);
  HAL_GPIO_WritePin(RS_P, rs ? GPIO_PIN_SET : GPIO_PIN_RESET);
  for (int i = 0; i < 8; i++)
    HAL_GPIO_WritePin(DB[i].port, DB[i].pin, (v >> i) & 1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
  us_wait();
  HAL_GPIO_WritePin(WR_P, GPIO_PIN_RESET); us_wait();
  HAL_GPIO_WritePin(WR_P, GPIO_PIN_SET);   us_wait();
}

static uint8_t r8(void)
{
  bus_mode(GPIO_MODE_INPUT);
  HAL_GPIO_WritePin(RS_P, GPIO_PIN_SET);
  HAL_GPIO_WritePin(RD_PORT, RD_PIN, GPIO_PIN_RESET);
  us_wait(); us_wait();
  uint8_t v = 0;
  for (int i = 0; i < 8; i++)
    if (HAL_GPIO_ReadPin(DB[i].port, DB[i].pin) == GPIO_PIN_SET) v |= (uint8_t)(1u << i);
  HAL_GPIO_WritePin(RD_PORT, RD_PIN, GPIO_PIN_SET);
  us_wait();
  return v;
}

static void rd_cmd(uint8_t c, uint8_t *out, int n)   /* command, then n bytes (first = dummy) */
{
  w8(0, c);
  for (int i = 0; i < n; i++) out[i] = r8();
}

static void tp3_print(const char *t) { HAL_UART_Transmit(&huart1, (uint8_t *)t, (uint16_t)strlen(t), 300); }

static void tft_read_test(void)
{
  char b[200];
  uint8_t id4[4], d3[4], pm[2], st[5];
  __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
  MX_USART1_UART_Init();

  for (;;)
  {
    tft_init();                                    /* also sets WR/RS as outputs */
    GPIO_InitTypeDef g = {0};
    g.Pin = RD_PIN; g.Mode = GPIO_MODE_OUTPUT_PP; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(RD_PORT, RD_PIN, GPIO_PIN_SET);
    HAL_GPIO_Init(RD_PORT, &g);

    rd_cmd(0x04, id4, 4);      /* Read Display ID: dummy + 3 bytes   */
    rd_cmd(0xD3, d3, 4);       /* ILI9341 ID4: dummy, 00, 93, 41      */
    rd_cmd(0x0A, pm, 2);       /* Power mode: dummy + value (9C = on) */
    rd_cmd(0x09, st, 5);       /* Status: dummy + 4 bytes             */
    bus_mode(GPIO_MODE_INPUT);
    HAL_UART_MspInit(&huart1);                     /* PA6 back to UART TX */

    snprintf(b, sizeof b,
             "\r\nTFT READ: ID(04) %02X %02X %02X | ID(D3) %02X %02X %02X | power(0A) %02X | status(09) %02X %02X %02X %02X\r\n",
             id4[1], id4[2], id4[3], d3[1], d3[2], d3[3], pm[1], st[1], st[2], st[3], st[4]);
    HAL_UART_Transmit(&huart1, (uint8_t *)b, (uint16_t)strlen(b), 300);
    if (d3[2] == 0x93 && d3[3] == 0x41)
      tp3_print("   -> ILI9341 answers: display alive, bus OK\r\n");
    else if ((id4[1] & id4[2] & id4[3] & d3[2] & d3[3]) == 0xFF)
      tp3_print("   -> all FF: nothing answers (no power to the display logic / RD not moved?)\r\n");
    else if ((id4[1] | id4[2] | id4[3] | d3[2] | d3[3]) == 0x00)
      tp3_print("   -> all 00: nothing answers (display logic not powered / CS not low?)\r\n");
    HAL_Delay(2000);
  }
}
#endif

#if (TFT_PIN_TEST == 2)
static void tft_colour_test(void)
{
  static const uint16_t col[] = { RED, GREEN, BLUE, BLACK, WHITE };
  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitTypeDef g = {0};
  g.Pin = GPIO_PIN_12; g.Mode = GPIO_MODE_OUTPUT_PP; g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &g);                       /* LD2 = "program alive" */

  HAL_Delay(500);
  for (;;)
  {
    tft_init();                                   /* repeated every round: in case the panel missed it */
    tft_set_fast(0);
    for (unsigned i = 0; i < sizeof col / sizeof col[0]; i++)
    {
      tft_fill_screen(col[i]);
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_12);
      HAL_Delay(1000);
    }
  }
}
#endif
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
  /* Config code for STM32_WPAN (HSE Tuning must be done before system clock configuration) */
  MX_APPE_Config();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* Configure the System Power */
  SystemPower_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_LPDMA1_Init();
  MX_RAMCFG_Init();
  MX_RTC_Init();
  MX_ICACHE_Init();
  /* USER CODE BEGIN 2 */
#if (TFT_PIN_TEST == 1)
  tft_pin_test();          /* wiring test - never returns (set TFT_PIN_TEST 0 for normal use) */
#elif (TFT_PIN_TEST == 2)
  tft_colour_test();       /* colour test - never returns */
#elif (TFT_PIN_TEST == 3)
  tft_read_test();         /* ID read test - never returns (LCD_RD on CN4-36) */
#endif

  /* USER CODE END 2 */

  /* Init code for STM32_WPAN */
  MX_APPE_Init(NULL);

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  /* BLE_TFT: start our part after the Bluetooth stack (MX_APPE_Init) is running.
     sensors_init() needs ST's ADC manager, which MX_APPE_Init sets up. */
  tft_init();
  lvgl_port_init();
  input_port_init();        /* button B1 -> LVGL */
  sensors_init();           /* chip temperature + supply voltage, 1 s (via adc_ctrl) */
  ui_create();              /* home menu */
  while (1)
  {

    /* USER CODE END WHILE */
    MX_APPE_Process();

    /* USER CODE BEGIN 3 */
    /* BLE_TFT: MX_APPE_Process() above runs ST's task scheduler (Bluetooth work);
       then LVGL redraws what changed and runs its timers.
       Step 6: while a picture from the phone is being drawn, LVGL must wait
       (it would move the display window), so it is skipped. */
    if (!ble_image_busy())
      lv_timer_handler();
    ams_client_poll();      /* step 7: iPhone media service steps */
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

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_MEDIUMLOW);

  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE
                              |RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEDiv = RCC_HSE_DIV1;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  /* BLE_TFT step 5.4: 64 MHz from the 32 MHz radio crystal through PLL1
     (same values as ST's BLE_TransparentMode_Ux_CDC example for this chip):
     32 MHz / M(2) = 16 MHz * N(24) = 384 MHz VCO / R(6) = 64 MHz SYSCLK
     (P = 48 MHz and Q = 64 MHz outputs are not used here). Was: PLL off, SYSCLK = HSE 32 MHz. */
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL1.PLLM = 2;
  RCC_OscInitStruct.PLL1.PLLN = 24;
  RCC_OscInitStruct.PLL1.PLLP = 8;
  RCC_OscInitStruct.PLL1.PLLQ = 6;
  RCC_OscInitStruct.PLL1.PLLR = 6;
  RCC_OscInitStruct.PLL1.PLLFractional = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK7|RCC_CLOCKTYPE_HCLK5;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;          /* BLE_TFT 5.4: was HSE */
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB7CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHB5_PLL1_CLKDivider = RCC_SYSCLK_PLL1_DIV2;     /* BLE_TFT 5.4: radio bus stays 32 MHz */
  RCC_ClkInitStruct.AHB5_HSEHSI_CLKDivider = RCC_SYSCLK_HSEHSI_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)   /* BLE_TFT 5.4: 1 wait state above 32 MHz */
  {
    Error_Handler();
  }

   /* Select SysTick source clock */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK_DIV8);           /* BLE_TFT 5.4: as in ST's 64 MHz example (was LSE) */

   /* Re-Initialize Tick with new clock source */
  if (HAL_InitTick(TICK_INT_PRIORITY) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RADIOST;
  PeriphClkInit.RadioSlpTimClockSelection = RCC_RADIOSTCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Power Configuration
  * @retval None
  */
static void SystemPower_Config(void)
{
  /* WKUP_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(WKUP_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(WKUP_IRQn);
/* USER CODE BEGIN PWR */

/* USER CODE END PWR */
}

/**
  * @brief ADC4 Initialization Function
  * @param None
  * @retval None
  */
void MX_ADC4_Init(void)
{

  /* USER CODE BEGIN ADC4_Init 0 */

  /* USER CODE END ADC4_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC4_Init 1 */

  /* USER CODE END ADC4_Init 1 */

  /** Common config
  */
  hadc4.Instance = ADC4;
  hadc4.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc4.Init.Resolution = ADC_RESOLUTION_12B;
  hadc4.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc4.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc4.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc4.Init.LowPowerAutoPowerOff = DISABLE;
  hadc4.Init.LowPowerAutonomousDPD = ADC_LP_AUTONOMOUS_DPD_DISABLE;
  hadc4.Init.LowPowerAutoWait = DISABLE;
  hadc4.Init.ContinuousConvMode = DISABLE;
  hadc4.Init.NbrOfConversion = 1;
  hadc4.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc4.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc4.Init.DMAContinuousRequests = DISABLE;
  hadc4.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_LOW;
  hadc4.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc4.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_814CYCLES_5;
  hadc4.Init.SamplingTimeCommon2 = ADC_SAMPLETIME_1CYCLE_5;
  hadc4.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
  if (HAL_ADC_ConfigChannel(&hadc4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC4_Init 2 */

  /* USER CODE END ADC4_Init 2 */

}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_DISABLE;
  hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
  hcrc.Init.GeneratingPolynomial = 7607;
  hcrc.Init.CRCLength = CRC_POLYLENGTH_16B;
  hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
  hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_WORDS;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */

  /** Enable instruction cache in 1-way (direct mapped cache)
  */
  LL_ICACHE_SetMode(LL_ICACHE_1WAY);
  LL_ICACHE_Enable();
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

/**
  * @brief LPDMA1 Initialization Function
  * @param None
  * @retval None
  */
void MX_LPDMA1_Init(void)
{

  /* USER CODE BEGIN LPDMA1_Init 0 */

  /* USER CODE END LPDMA1_Init 0 */

  /* Peripheral clock enable */
  __HAL_RCC_LPDMA1_CLK_ENABLE();

  /* LPDMA1 interrupt Init */
    HAL_NVIC_SetPriority(LPDMA1_Channel0_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(LPDMA1_Channel0_IRQn);
    HAL_NVIC_SetPriority(LPDMA1_Channel1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(LPDMA1_Channel1_IRQn);

  /* USER CODE BEGIN LPDMA1_Init 1 */

  /* USER CODE END LPDMA1_Init 1 */
  /* USER CODE BEGIN LPDMA1_Init 2 */

  /* USER CODE END LPDMA1_Init 2 */

}

/**
  * @brief RAMCFG Initialization Function
  * @param None
  * @retval None
  */
void MX_RAMCFG_Init(void)
{

  /* USER CODE BEGIN RAMCFG_Init 0 */

  /* USER CODE END RAMCFG_Init 0 */

  /* USER CODE BEGIN RAMCFG_Init 1 */

  /* USER CODE END RAMCFG_Init 1 */

  /** Initialize RAMCFG SRAM1
  */
  hramcfg_SRAM1.Instance = RAMCFG_SRAM1;
  if (HAL_RAMCFG_Init(&hramcfg_SRAM1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_RAMCFG_ConfigWaitState(&hramcfg_SRAM1, RAMCFG_WAITSTATE_0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RAMCFG_Init 2 */

  /* USER CODE END RAMCFG_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_PrivilegeStateTypeDef privilegeState = {0};
  RTC_AlarmTypeDef sAlarm = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = 31;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
  hrtc.Init.BinMode = RTC_BINARY_ONLY;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  privilegeState.rtcPrivilegeFull = RTC_PRIVILEGE_FULL_NO;
  privilegeState.backupRegisterPrivZone = RTC_PRIVILEGE_BKUP_ZONE_NONE;
  privilegeState.backupRegisterStartZone2 = RTC_BKP_DR0;
  privilegeState.backupRegisterStartZone3 = RTC_BKP_DR0;
  if (HAL_RTCEx_PrivilegeModeSet(&hrtc, &privilegeState) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  if (HAL_RTCEx_SetSSRU_IT(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the Alarm A
  */
  sAlarm.BinaryAutoClr = RTC_ALARMSUBSECONDBIN_AUTOCLR_NO;
  sAlarm.AlarmTime.SubSeconds = 0x0;
  sAlarm.AlarmMask = RTC_ALARMMASK_NONE;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDBINMASK_NONE;
  sAlarm.Alarm = RTC_ALARM_A;
  sAlarm.FlagAutoClr = ALARM_FLAG_AUTOCLR_DISABLE;
  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
void MX_USART1_UART_Init(void)
{

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
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param None
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
  Error_Handler();
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
