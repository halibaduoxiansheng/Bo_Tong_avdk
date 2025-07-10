// Copyright 2020-2021 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <driver/int.h>
#include <os/mem.h>
#include <os/os.h>
#include <os/str.h>

#include "driver/lcd.h"
#include "lcd_panel_devices.h"

/**
 * 屏幕接口类型 speed ： MCU(0-10Mbps) < SPI(几十) < QSPI(上百) < RGB(几百或更多)
 */
const lcd_device_t *lcd_devices[] =
{
#if CONFIG_LCD_ST7282
	&lcd_device_st7282, // RGB565
#endif

#if CONFIG_LCD_HX8282
	&lcd_device_hx8282, // RGB565
#endif

#if CONFIG_LCD_ST7796S
	&lcd_device_st7796s, // MCU8080
#endif

#if CONFIG_LCD_GC9503V
	&lcd_device_gc9503v, // RGB565
#endif

#if CONFIG_LCD_NT35512
	&lcd_device_nt35512, // RGB565
#endif

#if CONFIG_LCD_NT35510
	&lcd_device_nt35510, // RGB565
#endif

#if CONFIG_LCD_NT35510_MCU
	&lcd_device_nt35510_mcu, // MCU8080
#endif

#if CONFIG_LCD_H050IWV
	&lcd_device_h050iwv, // RGB565 
#endif

#if CONFIG_LCD_MD0430R
	&lcd_device_md0430r, // RGB565
#endif

#if CONFIG_LCD_MD0700R
	&lcd_device_md0700r, // RGB565
#endif

#if CONFIG_LCD_ST7701S_LY
	&lcd_device_st7701s_ly, // RGB565
#endif

#if CONFIG_LCD_ST7701S
	&lcd_device_st7701s, // RGB565
#endif

#if CONFIG_LCD_ST7701SN
	&lcd_device_st7701sn, // RGB565
#endif

#if CONFIG_LCD_ST7789V
	&lcd_device_st7789v, // MCU8080
#endif

#if CONFIG_LCD_AML01
	&lcd_device_aml01, // RGB
#endif

#if CONFIG_LCD_QSPI_SH8601A
	&lcd_device_sh8601a, // QSPI
#endif

#if CONFIG_LCD_QSPI_ST77903_WX20114
	&lcd_device_st77903_wx20114, // QSPI
#endif

#if CONFIG_LCD_QSPI_ST77903_SAT61478M
	&lcd_device_st77903_sat61478m, // QSPI
#endif

#if CONFIG_LCD_QSPI_ST77903_H0165Y008T
	&lcd_device_st77903_h0165y008t, // QSPI
#endif

#if CONFIG_LCD_QSPI_SPD2010
	&lcd_device_spd2010, // QSPI
#endif

#if CONFIG_LCD_QSPI_GC9C01
	&lcd_device_gc9c01, // QSPI
#endif

#if CONFIG_LCD_SPI_ST7796U
	&lcd_device_st7796u, // SPI
#endif

#if CONFIG_LCD_SPI_GC9D01 /*开发板*/
	&lcd_device_gc9d01, // SPI
#endif

};


void lcd_panel_devices_init(void)
{
	bk_lcd_set_devices_list(&lcd_devices[0], sizeof(lcd_devices) / sizeof(lcd_device_t *));
}
