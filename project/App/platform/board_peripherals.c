#include "board_gpio.h"
/* Board wiring from LXB743ZI-P1 and vendor examples 3/6/7/8/4/5/11.
 * The running project's STM32CubeH7 HAL is the only HAL implementation. */
#include "board_peripherals.h"
#include "board_config.h"
#include "integration_config.h"
static RTC_HandleTypeDef s_rtc;
static TIM_HandleTypeDef s_pwm;
static uint8_t s_pwm_started[2];
static uint8_t s_pwm_initialized;

/* Safe pre-init levels formerly duplicated as hardcoded GPIOG pins in main.c. */
void BoardStartupGpio_Init(void)
{
 GPIO_InitTypeDef g={0};
 BoardGpio_EnableClock(BOARD_LED_HEARTBEAT_PORT);BoardGpio_EnableClock(BOARD_LCD_SPI_PORT);
 BoardGpio_EnableClock(BOARD_LCD_BL_PORT);BoardGpio_EnableClock(BOARD_LCD_DC_PORT);
 HAL_GPIO_WritePin(BOARD_LED_HEARTBEAT_PORT,BOARD_LED_HEARTBEAT_PIN,BOARD_LED_HEARTBEAT_ACTIVE_LOW?GPIO_PIN_SET:GPIO_PIN_RESET);
 HAL_GPIO_WritePin(BOARD_LCD_SPI_PORT,BOARD_LCD_SPI_NSS,GPIO_PIN_SET);
 HAL_GPIO_WritePin(BOARD_LCD_BL_PORT,BOARD_LCD_BL_PIN,GPIO_PIN_RESET);
 HAL_GPIO_WritePin(BOARD_LCD_DC_PORT,BOARD_LCD_DC_PIN,GPIO_PIN_RESET);
 g.Mode=GPIO_MODE_OUTPUT_PP;g.Pull=GPIO_NOPULL;g.Speed=GPIO_SPEED_FREQ_LOW;
 g.Pin=BOARD_LED_HEARTBEAT_PIN;HAL_GPIO_Init(BOARD_LED_HEARTBEAT_PORT,&g);
 g.Pin=BOARD_LCD_SPI_NSS;HAL_GPIO_Init(BOARD_LCD_SPI_PORT,&g);
 g.Pin=BOARD_LCD_BL_PIN;HAL_GPIO_Init(BOARD_LCD_BL_PORT,&g);
 g.Pin=BOARD_LCD_DC_PIN;HAL_GPIO_Init(BOARD_LCD_DC_PORT,&g);
 g.Mode=GPIO_MODE_AF_PP;g.Alternate=BOARD_LCD_SPI_AF;g.Pin=BOARD_LCD_SPI_SCK|BOARD_LCD_SPI_MOSI;
 HAL_GPIO_Init(BOARD_LCD_SPI_PORT,&g);
}

void BoardMemory_Configure(void)
{
    MPU_Region_InitTypeDef m = {0};
    HAL_MPU_Disable();
    m.Enable = MPU_REGION_ENABLE; m.Number = MPU_REGION_NUMBER1;
    m.BaseAddress = 0x24000000; m.Size = MPU_REGION_SIZE_512KB;
    m.TypeExtField = MPU_TEX_LEVEL1; m.AccessPermission = MPU_REGION_FULL_ACCESS;
    m.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE; m.IsShareable = MPU_ACCESS_SHAREABLE;
    m.IsCacheable = MPU_ACCESS_NOT_CACHEABLE; m.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&m);
    m.Number = MPU_REGION_NUMBER2; m.BaseAddress = 0x90000000; m.Size = MPU_REGION_SIZE_8MB;
    m.AccessPermission = MPU_REGION_PRIV_RO_URO; HAL_MPU_ConfigRegion(&m);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

HAL_StatusTypeDef BoardPeripherals_ClockInit(void)
{
    RCC_PeriphCLKInitTypeDef p = {0};
    RCC_OscInitTypeDef osc = {0};
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
    osc.HSI48State = RCC_HSI48_ON;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) return HAL_ERROR;
    p.PeriphClockSelection = BOARD_LCD_RCC_SELECTION | BOARD_QSPI_RCC_SELECTION | BOARD_SD_RCC_SELECTION | BOARD_USB_RCC_SELECTION;
    BOARD_LCD_RCC_CONFIG(p);
    BOARD_QSPI_RCC_CONFIG(p);
    BOARD_SD_RCC_CONFIG(p);
    BOARD_USB_RCC_CONFIG(p);
    /* 25MHz /5 *80 =400MHz VCO; PLL2R=100MHz. SD ClockDiv=2 ->25MHz. */
    p.PLL2.PLL2M = 5; p.PLL2.PLL2N = 80;
    p.PLL2.PLL2P = 8; p.PLL2.PLL2Q = 4; p.PLL2.PLL2R = 4;
    p.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_2;
    p.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    p.PLL2.PLL2FRACN = 0;
    return HAL_RCCEx_PeriphCLKConfig(&p);
}

HAL_StatusTypeDef BoardRtc_Init(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_PeriphCLKInitTypeDef p = {0};
    HAL_PWR_EnableBkUpAccess();
    osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    osc.LSEState = RCC_LSE_ON;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) return HAL_ERROR;
    p.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    p.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&p) != HAL_OK) return HAL_ERROR;
    __HAL_RCC_RTC_ENABLE();
    s_rtc.Instance = RTC;
    s_rtc.Init.HourFormat = RTC_HOURFORMAT_24;
    s_rtc.Init.AsynchPrediv = 127; s_rtc.Init.SynchPrediv = 255;
    s_rtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    s_rtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    s_rtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    if (HAL_RTC_Init(&s_rtc) != HAL_OK) return HAL_ERROR;
    if (HAL_RTCEx_BKUPRead(&s_rtc, RTC_BKP_DR0) != 0x7431U)
        return BoardRtc_Set(2026, 1, 1, 0, 0, 0); /* unset default; console can set wall time */
    return HAL_OK;
}

HAL_StatusTypeDef BoardRtc_Set(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    RTC_TimeTypeDef t = {0}; RTC_DateTypeDef d = {0};
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint8_t maxday;
    static const uint8_t month_offset[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    uint32_t weekday, calendar_year;
    if (s_rtc.Instance != RTC) return HAL_ERROR;
    if (year < 2000 || year > 2099 || month < 1 || month > 12 || hour > 23 || minute > 59 || second > 59) return HAL_ERROR;
    maxday = days[month - 1U];
    if (month == 2 && year % 4 == 0) maxday++;
    if (!day || day > maxday) return HAL_ERROR;
    t.Hours = hour; t.Minutes = minute; t.Seconds = second;
    t.DayLightSaving = RTC_DAYLIGHTSAVING_NONE; t.StoreOperation = RTC_STOREOPERATION_RESET;
    d.Year = (uint8_t)(year - 2000); d.Month = month; d.Date = day; calendar_year = year - (month < 3U ? 1U : 0U);
    weekday = (calendar_year + calendar_year/4U - calendar_year/100U + calendar_year/400U + month_offset[month-1U] + day) % 7U;
    d.WeekDay = weekday ? (uint8_t)weekday : RTC_WEEKDAY_SUNDAY;
    if (HAL_RTC_SetTime(&s_rtc, &t, RTC_FORMAT_BIN) != HAL_OK || HAL_RTC_SetDate(&s_rtc, &d, RTC_FORMAT_BIN) != HAL_OK) return HAL_ERROR;
    HAL_RTCEx_BKUPWrite(&s_rtc, RTC_BKP_DR0, 0x7431U);
    return HAL_OK;
}

HAL_StatusTypeDef BoardRtc_Get(RTC_TimeTypeDef *t, RTC_DateTypeDef *d)
{
    if (!t || !d || s_rtc.Instance != RTC) return HAL_ERROR;
    if (HAL_RTC_GetTime(&s_rtc, t, RTC_FORMAT_BIN) != HAL_OK) return HAL_ERROR;
    return HAL_RTC_GetDate(&s_rtc, d, RTC_FORMAT_BIN); /* unlock shadow registers */
}

HAL_StatusTypeDef BoardPwm_SetChannelUs(uint8_t channel, uint16_t pulse_us)
{
#if BOARD_ENABLE_PWM
    GPIO_InitTypeDef g = {0}; TIM_OC_InitTypeDef oc = {0};
    uint32_t timer_channel = channel == 0 ? TIM_CHANNEL_1 : TIM_CHANNEL_2;
    if (channel > 1 || pulse_us < 500 || pulse_us > 2500) return HAL_ERROR;
    if (!s_pwm_initialized) {
        BOARD_PWM_CLOCK_ENABLE(); BoardGpio_EnableClock(BOARD_PWM_PORT);
        if (HAL_RCC_GetPCLK1Freq() != BOARD_EXPECTED_PCLK1_HZ) return HAL_ERROR;
        s_pwm.Instance = BOARD_PWM_INSTANCE;
        s_pwm.Init.Prescaler = BOARD_PWM_KERNEL_HZ / BOARD_PWM_COUNTER_HZ - 1U;
        s_pwm.Init.CounterMode = TIM_COUNTERMODE_UP; s_pwm.Init.Period = BOARD_PWM_PERIOD_US - 1U;
        s_pwm.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        s_pwm.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        if (HAL_TIM_PWM_Init(&s_pwm) != HAL_OK) return HAL_ERROR;
        s_pwm_initialized = 1U;
    }
    if (!s_pwm_started[channel]) {
        g.Pin = channel == 0 ? BOARD_PWM_PIN : BOARD_PWM2_PIN;
        g.Mode = GPIO_MODE_AF_PP; g.Alternate = BOARD_PWM_AF;
        g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW; HAL_GPIO_Init(BOARD_PWM_PORT,&g);
        oc.OCMode = TIM_OCMODE_PWM1; oc.Pulse = pulse_us;
        oc.OCPolarity = TIM_OCPOLARITY_HIGH; oc.OCFastMode = TIM_OCFAST_DISABLE;
        if (HAL_TIM_PWM_ConfigChannel(&s_pwm,&oc,timer_channel) != HAL_OK || HAL_TIM_PWM_Start(&s_pwm,timer_channel) != HAL_OK) return HAL_ERROR;
        s_pwm_started[channel] = 1U;
    }
    __HAL_TIM_SET_COMPARE(&s_pwm,timer_channel,pulse_us); return HAL_OK;
#else
    (void)channel; (void)pulse_us; return HAL_ERROR;
#endif
}
HAL_StatusTypeDef BoardPwm_SetUs(uint16_t pulse_us) { return BoardPwm_SetChannelUs(0,pulse_us); }
void BoardPwm_Stop(void)
{
    if (s_pwm_started[0]) (void)HAL_TIM_PWM_Stop(&s_pwm,TIM_CHANNEL_1);
    if (s_pwm_started[1]) (void)HAL_TIM_PWM_Stop(&s_pwm,TIM_CHANNEL_2);
    s_pwm_started[0]=s_pwm_started[1]=0;
}
