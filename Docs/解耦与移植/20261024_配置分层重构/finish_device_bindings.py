from pathlib import Path
import re
H=Path(__file__).resolve().parents[3];A=H/'project/App';B=A/'platform/boards/lxb743zi_p1'
def put(p,s):p.write_text(s,encoding='utf8')
def add(p,body):s=p.read_text(encoding='utf8');i=s.rindex('#endif');put(p,s[:i]+body+'\n'+s[i:])
add(B/'board_vision_cfg.h','''#define BOARD_LCD_RCC_SELECTION RCC_PERIPHCLK_SPI6
#define BOARD_LCD_RCC_CONFIG(p) ((p).Spi6ClockSelection=RCC_SPI6CLKSOURCE_D3PCLK1)
#define BOARD_CAMERA_INSTANCE DCMI
#define BOARD_CAMERA_CLOCK_ENABLE() __HAL_RCC_DCMI_CLK_ENABLE()
#define BOARD_CAMERA_IRQn DCMI_IRQn
#define BOARD_CAMERA_HANDLER DCMI_IRQHandler
#define BOARD_CAMERA_PRIORITY 7U
#define BOARD_CAMERA_DMA_INSTANCE DMA2_Stream7
#define BOARD_CAMERA_DMA_REQUEST DMA_REQUEST_DCMI
#define BOARD_CAMERA_DMA_CLOCK_ENABLE() __HAL_RCC_DMA2_CLK_ENABLE()
#define BOARD_CAMERA_DMA_IRQn DMA2_Stream7_IRQn
#define BOARD_CAMERA_DMA_HANDLER DMA2_Stream7_IRQHandler
''')
add(B/'board_storage_cfg.h','''#define BOARD_SD_INSTANCE SDMMC1
#define BOARD_SD_IRQn SDMMC1_IRQn
#define BOARD_SD_HANDLER SDMMC1_IRQHandler
#define BOARD_SD_PRIORITY 14U
#define BOARD_SD_CLOCK_ENABLE() __HAL_RCC_SDMMC1_CLK_ENABLE()
#define BOARD_SD_CLOCK_DISABLE() __HAL_RCC_SDMMC1_CLK_DISABLE()
#define BOARD_SD_FORCE_RESET() __HAL_RCC_SDMMC1_FORCE_RESET()
#define BOARD_SD_RELEASE_RESET() __HAL_RCC_SDMMC1_RELEASE_RESET()
#define BOARD_SD_RCC_SELECTION RCC_PERIPHCLK_SDMMC
#define BOARD_SD_RCC_CONFIG(p) ((p).SdmmcClockSelection=RCC_SDMMCCLKSOURCE_PLL2)
#define BOARD_QSPI_CLOCK_ENABLE() __HAL_RCC_QSPI_CLK_ENABLE()
#define BOARD_QSPI_FORCE_RESET() __HAL_RCC_QSPI_FORCE_RESET()
#define BOARD_QSPI_RELEASE_RESET() __HAL_RCC_QSPI_RELEASE_RESET()
#define BOARD_QSPI_RCC_SELECTION RCC_PERIPHCLK_QSPI
#define BOARD_QSPI_RCC_CONFIG(p) ((p).QspiClockSelection=RCC_QSPICLKSOURCE_D1HCLK)
''')
add(B/'board_serial_cfg.h','''/* Native USB CDC is a different transport from HOST UART. */
#define BOARD_USB_INSTANCE USB_OTG_FS
#define BOARD_USB_GPIO_PORT GPIOA
#define BOARD_USB_GPIO_PINS (GPIO_PIN_11|GPIO_PIN_12)
#define BOARD_USB_GPIO_AF GPIO_AF10_OTG1_FS
#define BOARD_USB_CLOCK_ENABLE() __HAL_RCC_USB_OTG_FS_CLK_ENABLE()
#define BOARD_USB_CLOCK_DISABLE() __HAL_RCC_USB_OTG_FS_CLK_DISABLE()
#define BOARD_USB_IRQn OTG_FS_IRQn
#define BOARD_USB_HANDLER OTG_FS_IRQHandler
#define BOARD_USB_PRIORITY 8U
#define BOARD_USB_RCC_SELECTION RCC_PERIPHCLK_USB
#define BOARD_USB_RCC_CONFIG(p) ((p).UsbClockSelection=RCC_USBCLKSOURCE_HSI48)
''')
# Fold GPIO kernel-clock facts into their device groups.
p=A/'platform/board_peripherals.c';s=p.read_text(encoding='utf8').replace('RCC_PERIPHCLK_SPI6 | RCC_PERIPHCLK_QSPI | RCC_PERIPHCLK_SDMMC | RCC_PERIPHCLK_USB','BOARD_LCD_RCC_SELECTION | BOARD_QSPI_RCC_SELECTION | BOARD_SD_RCC_SELECTION | BOARD_USB_RCC_SELECTION')
for old,new in [('p.Spi6ClockSelection = RCC_SPI6CLKSOURCE_D3PCLK1','BOARD_LCD_RCC_CONFIG(p)'),('p.QspiClockSelection = RCC_QSPICLKSOURCE_D1HCLK','BOARD_QSPI_RCC_CONFIG(p)'),('p.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL2','BOARD_SD_RCC_CONFIG(p)'),('p.UsbClockSelection = RCC_USBCLKSOURCE_HSI48','BOARD_USB_RCC_CONFIG(p)')]:s=s.replace(old,new)
put(p,s)
for fn in ['dcmi_ov5640.c','dcmi_ov2640.c']:
 p=A/'vendor/camera'/fn;s=p.read_text(encoding='utf8');s=s.replace('__HAL_RCC_DCMI_CLK_ENABLE()','BOARD_CAMERA_CLOCK_ENABLE()').replace('__HAL_RCC_DMA2_CLK_ENABLE()','BOARD_CAMERA_DMA_CLOCK_ENABLE()')
 for old,new in [('DMA2_Stream7_IRQn','BOARD_CAMERA_DMA_IRQn'),('DMA2_Stream7','BOARD_CAMERA_DMA_INSTANCE'),('DMA_REQUEST_DCMI','BOARD_CAMERA_DMA_REQUEST'),('DCMI_IRQn','BOARD_CAMERA_IRQn')]:s=s.replace(old,new)
 s=re.sub(r'(Instance\s*(?:=|==)\s*)DCMI\b',r'\1BOARD_CAMERA_INSTANCE',s)
 s=re.sub(r'(HAL_NVIC_SetPriority\(BOARD_CAMERA_(?:DMA_)?IRQn,\s*)7',r'\1BOARD_CAMERA_PRIORITY',s);put(p,s)
for fn in ['dcmi_ov5640.h','dcmi_ov2640.h','sccb.h']:
 p=A/'vendor/camera'/fn;s=p.read_text(encoding='utf8');s='#include "board_gpio.h"\n'+s
 for n,port in [('GPIO_OV5640_PWDN_CLK_ENABLE','BOARD_CAM_PWDN_PORT'),('GPIO_OV2640_PWDN_CLK_ENABLE','BOARD_CAM_PWDN_PORT'),('GPIO_SCCB_SCL_CLK_ENABLE','BOARD_CAM_SCL_PORT'),('GPIO_SCCB_SDA_CLK_ENABLE','BOARD_CAM_SDA_PORT')]:
  s=re.sub(r'(#define\s+'+n+r'\s+)__HAL_RCC_GPIOF_CLK_ENABLE\(\)',r'\1BoardGpio_EnableClock('+port+')',s)
 put(p,s)
p=A/'vendor/qspi/qspi_w25q64.c';s=p.read_text(encoding='utf8').replace('hqspi->Instance==QUADSPI','hqspi->Instance==BOARD_QSPI_INSTANCE');s=s.replace('__HAL_RCC_QSPI_CLK_ENABLE()','BOARD_QSPI_CLOCK_ENABLE()').replace('__HAL_RCC_QSPI_FORCE_RESET()','BOARD_QSPI_FORCE_RESET()').replace('__HAL_RCC_QSPI_RELEASE_RESET()','BOARD_QSPI_RELEASE_RESET()');put(p,s)
p=A/'vendor/sd/sdmmc_sd.c';s=p.read_text(encoding='utf8');s=re.sub(r'\bSDMMC1_IRQn\b','BOARD_SD_IRQn',s);s=re.sub(r'\bSDMMC1\b','BOARD_SD_INSTANCE',s)
for field in ['CLK_ENABLE','CLK_DISABLE','FORCE_RESET','RELEASE_RESET']:s=s.replace(f'__HAL_RCC_SDMMC1_{field}()',f'BOARD_SD_{field.replace("CLK_","CLOCK_")}()')
s=s.replace('HAL_NVIC_SetPriority(BOARD_SD_IRQn, 14, 0)','HAL_NVIC_SetPriority(BOARD_SD_IRQn, BOARD_SD_PRIORITY, 0)');put(p,s)
p=H/'project/USB_DEVICE/Target/usbd_conf.c';s=p.read_text(encoding='utf8');s='#include "board_config.h"\n#include "board_gpio.h"\n'+s
s=re.sub(r'\bUSB_OTG_FS\b','BOARD_USB_INSTANCE',s).replace('__HAL_RCC_GPIOA_CLK_ENABLE()','BoardGpio_EnableClock(BOARD_USB_GPIO_PORT)').replace('GPIO_PIN_11|GPIO_PIN_12','BOARD_USB_GPIO_PINS').replace('GPIO_AF10_OTG1_FS','BOARD_USB_GPIO_AF').replace('HAL_GPIO_Init(GPIOA,','HAL_GPIO_Init(BOARD_USB_GPIO_PORT,').replace('HAL_GPIO_DeInit(GPIOA,','HAL_GPIO_DeInit(BOARD_USB_GPIO_PORT,')
s=s.replace('__HAL_RCC_USB_OTG_FS_CLK_ENABLE()','BOARD_USB_CLOCK_ENABLE()').replace('__HAL_RCC_USB_OTG_FS_CLK_DISABLE()','BOARD_USB_CLOCK_DISABLE()').replace('OTG_FS_IRQn','BOARD_USB_IRQn').replace('HAL_NVIC_SetPriority(BOARD_USB_IRQn, 8, 0)','HAL_NVIC_SetPriority(BOARD_USB_IRQn, BOARD_USB_PRIORITY, 0)')
s=s.replace('PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB','PeriphClkInitStruct.PeriphClockSelection = BOARD_USB_RCC_SELECTION').replace('PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48','BOARD_USB_RCC_CONFIG(PeriphClkInitStruct)');put(p,s)
p=H/'project/Core/Src/stm32h7xx_it.c';s=p.read_text(encoding='utf8')
for old,new in [('void DCMI_IRQHandler','void BOARD_CAMERA_HANDLER'),('void DMA2_Stream7_IRQHandler','void BOARD_CAMERA_DMA_HANDLER'),('void SDMMC1_IRQHandler','void BOARD_SD_HANDLER'),('void OTG_FS_IRQHandler','void BOARD_USB_HANDLER')]:s=s.replace(old,new)
put(p,s)
print('USB/display/camera/SD/QSPI resources and IRQ names now follow their grouped profiles')
