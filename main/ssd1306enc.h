/**
 * An extend of ssd1306 lib
 */

#ifndef _SSD1306ENC_H__H_
#define _SSD1306ENC_H__H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "driver/spi_master.h"
#include "fonts.h"
#include "ssd1306_1bit.h"

#ifdef __cplusplus
extern "C" {
#endif

//! @brief Drawing color
typedef enum {
	TRANSPARENT = -1, //!< Transparent (not drawing)
	BLACK = 0,        //!< Black (pixel off)
	WHITE = 1,        //!< White (or blue, yellow, pixel on)
	INVERT = 2,       //!< Invert pixel (XOR)
} ssd1306_color_t;

//! @brief Panel type
typedef enum {
	SSD1306_128x64 = 1,	//!< 128x32 panel
	SSD1306_128x32 = 2	//!< 128x64 panel
} ssd1306_panel_type_t;

//font type
typedef enum FONTNAME
{
	FT_HZ = 0,
	FT_HZ_EXT,
	FT_ASCII_5x7,
	FT_ASCII_7x8,
	FT_ASCII_8x16,
	FT_ASCII_8x16_ARIAL,
	FT_ASCII_8x16_BOLD,
	FT_ASCII_16x16_TIMES
} fontname_t;

typedef struct FONTADDR
{
    uint8_t high;
    uint8_t mid;
    uint8_t low;
} fontaddr_t;

typedef struct FONTINFO
{
	fontname_t name;
	uint8_t width;
	uint32_t offset;
} fontinfo_t;

#define PIN_FONT_CS 25
#define PIN_OLED_CS 5
#define NUM_FONTS_ZK 8

void spi_get_data_tobuf(spi_device_handle_t spi,fontaddr_t addr,uint8_t * buff);
void spi_get_data_tobuf_34(spi_device_handle_t spi,fontaddr_t addr,uint8_t * buff);
fontaddr_t getStingAddr(char *text);//only support 2byte
void    select_font(uint8_t idx);
uint8_t draw_char(uint8_t x, uint8_t y, unsigned char c);
uint8_t draw_string(uint8_t x, uint8_t y, char *str);
void draw_gb_string(uint8_t x,uint8_t y,char *text);
void drawBitmap_togram(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *buf);
void refreshex(uint8_t force);

#ifdef __cplusplus
}
#endif

// ----------------------------------------------------------------------------
#endif // 