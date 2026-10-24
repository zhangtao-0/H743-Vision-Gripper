#include "board_gpio.h"
void BoardGpio_EnableClock(GPIO_TypeDef *port)
{
#if defined(GPIOA)
 if(port==GPIOA){__HAL_RCC_GPIOA_CLK_ENABLE();return;}
#endif
#if defined(GPIOB)
 if(port==GPIOB){__HAL_RCC_GPIOB_CLK_ENABLE();return;}
#endif
#if defined(GPIOC)
 if(port==GPIOC){__HAL_RCC_GPIOC_CLK_ENABLE();return;}
#endif
#if defined(GPIOD)
 if(port==GPIOD){__HAL_RCC_GPIOD_CLK_ENABLE();return;}
#endif
#if defined(GPIOE)
 if(port==GPIOE){__HAL_RCC_GPIOE_CLK_ENABLE();return;}
#endif
#if defined(GPIOF)
 if(port==GPIOF){__HAL_RCC_GPIOF_CLK_ENABLE();return;}
#endif
#if defined(GPIOG)
 if(port==GPIOG){__HAL_RCC_GPIOG_CLK_ENABLE();return;}
#endif
#if defined(GPIOH)
 if(port==GPIOH){__HAL_RCC_GPIOH_CLK_ENABLE();return;}
#endif
#if defined(GPIOI)
 if(port==GPIOI){__HAL_RCC_GPIOI_CLK_ENABLE();return;}
#endif
#if defined(GPIOJ)
 if(port==GPIOJ){__HAL_RCC_GPIOJ_CLK_ENABLE();return;}
#endif
#if defined(GPIOK)
 if(port==GPIOK){__HAL_RCC_GPIOK_CLK_ENABLE();return;}
#endif
 (void)port;
}
