/*
 * oled_fonts.c
 *
 *  Created on: Jan 3, 2015
 *      Author: Baoshi
 */
//#include "esp_common.h"
#include "fonts.h"

extern const font_info_t tahoma_8ptFontInfo;

const font_info_t * fonts[NUM_FONTS] =
{
    &tahoma_8ptFontInfo
};
