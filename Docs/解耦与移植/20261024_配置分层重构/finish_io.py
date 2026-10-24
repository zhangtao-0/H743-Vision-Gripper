from pathlib import Path
import re
H=Path(__file__).resolve().parents[3];A=H/'project/App';B=A/'platform/boards/lxb743zi_p1'
def put(p,s):p.write_text(s,encoding='utf8')
# Preserve safe startup pin levels, but derive pins from their device profiles.
p=B/'board_vision_cfg.h';s=p.read_text(encoding='utf8');s=s.replace('#define BOARD_LCD_SPI_PINS (GPIO_PIN_8|GPIO_PIN_13|GPIO_PIN_14)','#define BOARD_LCD_SPI_NSS GPIO_PIN_8\n#define BOARD_LCD_SPI_SCK GPIO_PIN_13\n#define BOARD_LCD_SPI_MOSI GPIO_PIN_14\n#define BOARD_LCD_SPI_PINS (BOARD_LCD_SPI_NSS|BOARD_LCD_SPI_SCK|BOARD_LCD_SPI_MOSI)');s=s.replace('#endif','#define BOARD_LCD_CLOCK_ENABLE() __HAL_RCC_SPI6_CLK_ENABLE()\n#endif');put(p,s)
p=A/'platform/board_peripherals.h';s=p.read_text(encoding='utf8').replace('void BoardMemory_Configure(void);','void BoardMemory_Configure(void);\nvoid BoardStartupGpio_Init(void);');put(p,s)
p=A/'platform/board_peripherals.c';s=p.read_text(encoding='utf8');pos=s.index('void BoardMemory_Configure(void)');s=s[:pos]+'''/* Safe pre-init levels formerly duplicated as hardcoded GPIOG pins in main.c. */
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

'''+s[pos:];put(p,s)
p=H/'project/Core/Src/main.c';s=p.read_text(encoding='utf8').replace('  /* LED, key, LCD and serial adapters enable/configure their own GPIO ports. */','  BoardStartupGpio_Init();');put(p,s)
# Vendor-adapted copies use configured GPIO clocks; native vendor reference copies remain untouched.
for sensor in ['dcmi_ov5640.c','dcmi_ov2640.c']:
 p=A/'vendor/camera'/sensor;s=p.read_text(encoding='utf8');s='#include "board_gpio.h"\n'+s
 for c in 'EDGCA':s=s.replace(f'__HAL_RCC_GPIO{c}_CLK_ENABLE();',f'BoardGpio_EnableClock(BOARD_CAM_{c}_PORT);')
 s=s.replace('__HAL_RCC_GPIOF_CLK_ENABLE();','BoardGpio_EnableClock(BOARD_CAM_PWDN_PORT);BoardGpio_EnableClock(BOARD_CAM_SCL_PORT);BoardGpio_EnableClock(BOARD_CAM_SDA_PORT);');put(p,s)
p=A/'vendor/lcd/lcd_spi_154.c';s=p.read_text(encoding='utf8');s='#include "board_gpio.h"\n'+s;s=s.replace('hspi->Instance==SPI6','hspi->Instance==BOARD_LCD_INSTANCE').replace('__HAL_RCC_SPI6_CLK_ENABLE();','BOARD_LCD_CLOCK_ENABLE();').replace('__HAL_RCC_GPIOG_CLK_ENABLE();','BoardGpio_EnableClock(BOARD_LCD_SPI_PORT);BoardGpio_EnableClock(BOARD_LCD_BL_PORT);BoardGpio_EnableClock(BOARD_LCD_DC_PORT);');put(p,s)
p=A/'vendor/lcd/lcd_spi_154.h';s=p.read_text(encoding='utf8');s='#include "board_gpio.h"\n'+s
s=re.sub(r'(#define\s+GPIO_LDC_Backlight_CLK_ENABLE\s+)__HAL_RCC_GPIOG_CLK_ENABLE\(\)',r'\1BoardGpio_EnableClock(BOARD_LCD_BL_PORT)',s)
s=re.sub(r'(#define\s+GPIO_LDC_DC_CLK_ENABLE\s+)__HAL_RCC_GPIOG_CLK_ENABLE\(\)',r'\1BoardGpio_EnableClock(BOARD_LCD_DC_PORT)',s);put(p,s)
p=A/'vendor/qspi/qspi_w25q64.h';s=p.read_text(encoding='utf8');s='#include "board_gpio.h"\n'+s
names={'GPIO_QUADSPI_CLK_ENABLE':'BOARD_QSPI_CLK_PORT','GPIO_QUADSPI_BK1_NCS_ENABLE':'BOARD_QSPI_CS_PORT',**{f'GPIO_QUADSPI_BK1_IO{i}_ENABLE':f'BOARD_QSPI_IO{i}_PORT' for i in range(4)}}
for n,port in names.items():s=re.sub(r'(#define\s+'+n+r'\s+)__HAL_RCC_GPIO[FG]_CLK_ENABLE\(\)',r'\1BoardGpio_EnableClock('+port+')',s)
put(p,s)
p=A/'vendor/sd/sdmmc_sd.c';s=p.read_text(encoding='utf8');s='#include "board_gpio.h"\n'+s
s=s.replace('__HAL_RCC_GPIOC_CLK_ENABLE();','BoardGpio_EnableClock(BOARD_SD_DATA_PORT);').replace('__HAL_RCC_GPIOD_CLK_ENABLE();','BoardGpio_EnableClock(BOARD_SD_CMD_PORT);').replace('HAL_GPIO_DeInit(GPIOC,','HAL_GPIO_DeInit(BOARD_SD_DATA_PORT,').replace('HAL_GPIO_DeInit(GPIOD,','HAL_GPIO_DeInit(BOARD_SD_CMD_PORT,');put(p,s)
# Never interpret a failed second read as an empty valid slot.
p=A/'platform/claw_limit_storage_h7.c';s=p.read_text(encoding='utf8');a=s.index('static const Record *Latest(');b=s.index('HAL_StatusTypeDef ClawLimitStorage_Load',a)
s=s[:a]+'''static int ReadRecords(Record records[2])
{return BoardNv_Read(BOARD_NV_LIMIT_A,&records[0],sizeof(Record))&&BoardNv_Read(BOARD_NV_LIMIT_B,&records[1],sizeof(Record));}
static const Record *Latest(Record records[2])
{
 const Record *a=&records[0],*b=&records[1];uint8_t va=Valid(a),vb=Valid(b);
 if(!va)return vb?b:0;if(!vb)return a;
 return (int32_t)(b->w[2]-a->w[2])>0?b:a;
}

'''+s[b:];s=s.replace('if (!out) return HAL_ERROR;','if (!out || !ReadRecords(records)) return HAL_ERROR;').replace('if(!BoardNv_Read(BOARD_NV_LIMIT_A,&records[0],sizeof(Record))||!BoardNv_Read(BOARD_NV_LIMIT_B,&records[1],sizeof(Record)))return HAL_ERROR;','if(!ReadRecords(records))return HAL_ERROR;');put(p,s)
print('Safe startup levels retained; configured GPIO clocks followed by display/camera/storage adapters')
