/**
 * A new lib of ssd1306
 */
#include "ssd1306enc.h"
#include "driver/gpio.h"
#include "freertos/event_groups.h"
#include "soc/gpio_struct.h"
#include "esp_log.h"

ssd1306_panel_type_t type;
uint8_t address;            // I2C address
uint8_t *buffer;            // display buffer
uint8_t width;              // panel width (128)
uint8_t height;             // panel height (32 or 64)
uint8_t refresh_top = 255;  // "Dirty" window
uint8_t refresh_left = 255;
uint8_t refresh_right = 0;
uint8_t refresh_bottom = 0;
const font_info_t *font;
static uint8_t gbuf[1024];
static uint8_t vbuf[1024];
extern spi_device_handle_t spifont;
static const char *TAG = "SSD1306ENC";

void spi_cmd(spi_device_handle_t spi, const uint8_t cmd) {
  esp_err_t ret;
  spi_transaction_t t;

  memset(&t, 0, sizeof(t));            // Zero out the transaction
  t.length = 8;                        // Command is 8 bits
  t.tx_buffer = &cmd;                  // The data is the cmd itself
  t.user = (void *)0;                  // D/C needs to be set to 0
  ret = spi_device_transmit(spi, &t);  // Transmit!
  assert(ret == ESP_OK);               // Should have had no issues.
}

// Send data to the LCD. Uses spi_device_transmit, which waits until the
// transfer is complete.
void spi_data(spi_device_handle_t spi, const uint8_t *data, int len) {
  esp_err_t ret;
  spi_transaction_t t;

  if (len == 0) {
    return;  // no need to send anything
  }
  memset(&t, 0, sizeof(t));  // Zero out the transaction
  t.length = len * 8;        // Len is in bytes, transaction length is in bits.
  t.tx_buffer = data;        // Data
  t.user = (void *)1;        // D/C needs to be set to 1
  ret = spi_device_transmit(spi, &t);  // Transmit!
  assert(ret == ESP_OK);               // Should have had no issues.
}

uint8_t fontbuf[32];
uint8_t *spi_get_data(spi_device_handle_t spi, fontaddr_t addr) {
  // uint8_t fontbuf[32];
  // get_id cmd
  // Reset the display
  gpio_set_level(PIN_FONT_CS, 0);
  gpio_set_level(PIN_OLED_CS, 1);

  spi_cmd(spi, 0x03);

  spi_cmd(spi, addr.high);
  spi_cmd(spi, addr.mid);
  spi_cmd(spi, addr.low);

  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = 8 * 32;
  // t.flags = SPI_TRANS_USE_RXDATA;
  t.user = (void *)1;
  t.rx_buffer = fontbuf;

  esp_err_t ret = spi_device_transmit(spi, &t);
  assert(ret == ESP_OK);

  gpio_set_level(PIN_FONT_CS, 1);
  gpio_set_level(PIN_OLED_CS, 0);
  return fontbuf;
}

uint32_t fontaddr = 0;
fontaddr_t getStingAddr(char *text)  // only support 2byte
{
  uint8_t i = 0;
  uint8_t addrHigh, addrMid, addrLow;
  // uint8_t fontbuf[32];
  fontaddr_t addr;

  if (((text[i] >= 0xb0) && (text[i] <= 0xf7)) && (text[i + 1] >= 0xa1)) {
    /*国标简体（GB2312）汉字在亿阳字库IC中的地址由以下公式来计算：*/
    /*Address = ((MSB - 0xB0) * 94 + (LSB - 0xA1)+ 846)*32+ BaseAdd;BaseAdd=0*/
    /*由于担心8位单片机有乘法溢出问题，所以分三部取地址*/
    fontaddr = (text[i] - 0xb0) * 94;
    fontaddr += (text[i + 1] - 0xa1) + 846;
    fontaddr = (ulong)(fontaddr * 32);

    addrHigh = (fontaddr & 0xff0000) >> 16; /*地址的高8位,共24位*/
    addrMid = (fontaddr & 0xff00) >> 8;     /*地址的中8位,共24位*/
    addrLow = fontaddr & 0xff;              /*地址的低8位,共24位*/
    addr.high = addrHigh;
    addr.mid = addrMid;
    addr.low = addrLow;
  } else if (((text[i] >= 0xa1) && (text[i] <= 0xa3)) &&
             (text[i + 1] >= 0xa1)) {
    /*国标简体（GB2312）15x16点的字符在亿阳字库IC中的地址由以下公式来计算：*/
    /*Address = ((MSB - 0xa1) * 94 + (LSB - 0xA1))*32+ BaseAdd;BaseAdd=0*/
    /*由于担心8位单片机有乘法溢出问题，所以分三部取地址*/
    fontaddr = (text[i] - 0xa1) * 94;
    fontaddr += (text[i + 1] - 0xa1);
    fontaddr = (ulong)(fontaddr * 32);

    addrHigh = (fontaddr & 0xff0000) >> 16; /*地址的高8位,共24位*/
    addrMid = (fontaddr & 0xff00) >> 8;     /*地址的中8位,共24位*/
    addrLow = fontaddr & 0xff;              /*地址的低8位,共24位*/
    addr.high = addrHigh;
    addr.mid = addrMid;
    addr.low = addrLow;
  } else if ((text[i] >= 0x20) && (text[i] <= 0x7f)) {
    // unsigned char fontbuf[16];
    fontaddr = (text[i] - 0x20);
    fontaddr = (unsigned long)(fontaddr * 16);
    fontaddr = (unsigned long)(fontaddr + 0x3b7c0);//0x3cf80 bold , 0x3b7c0 regular
    addrHigh = (fontaddr & 0xff0000) >> 16;
    addrMid = (fontaddr & 0xff00) >> 8;
    addrLow = fontaddr & 0xff;
    addr.high = addrHigh;
    addr.mid = addrMid;
    addr.low = addrLow;
  }
  return addr;
}

void draw_gb_string(uint8_t x,uint8_t y,char *text)
{
  uint16_t i = 0;
  fontaddr_t addr;
  uint8_t fontbuf[34];
  while((text[i]>0x00))
  {

    if(((text[i]>=0xb0) &&(text[i]<=0xf7))&&(text[i+1]>=0xa1))
    {
      /*国标简体（GB2312）汉字在亿阳字库IC中的地址由以下公式来计算：*/
      /*Address = ((MSB - 0xB0) * 94 + (LSB - 0xA1)+ 846)*32+ BaseAdd;BaseAdd=0*/
      /*由于担心8位单片机有乘法溢出问题，所以分三部取地址*/
      fontaddr = (text[i]- 0xb0)*94;
      fontaddr += (text[i+1]-0xa1)+846;
      fontaddr = (ulong)(fontaddr*32);

      addr.high = (fontaddr&0xff0000)>>16;  /*地址的高8位,共24位*/
      addr.mid = (fontaddr&0xff00)>>8;      /*地址的中8位,共24位*/
      addr.low = fontaddr&0xff;       /*地址的低8位,共24位*/
      if(x + 16 > 128) {
        //new line
        x = 0;
        y += 16;
        if(y > 63) {
          break;
        }
      }
      //get_n_bytes_data_from_ROM(addrHigh,addrMid,addrLow,fontbuf,32 );/*取32个字节的数据，存到"fontbuf[32]"*/
      //display_graphic_16x16(y,x,fontbuf);/*显示汉字到LCD上，y为页地址，x为列地址，fontbuf[]为数据*/
      spi_get_data_tobuf(spifont, addr, fontbuf);
      ESP_LOGI(TAG,"draw_gb_string zone 1 Current index:%d ==  CHAR: %x%x ,pos[%d,%d]",i,text[i],text[i+1],x,y);
      printf("CHAR: ");
      for (int i = 0; i < strlen(text); ++i)
      {
        /* code */
        printf("%x", text[i]);
      }
      printf("\n");
      drawBitmap_togram(x, y, 16, 16, fontbuf);
      i+=2;
      x+=16;
    }
    else if(((text[i]>=0xa1) &&(text[i]<=0xa3))&&(text[i+1]>=0xa1))
    {
      /*国标简体（GB2312）15x16点的字符在亿阳字库IC中的地址由以下公式来计算：*/
      /*Address = ((MSB - 0xa1) * 94 + (LSB - 0xA1))*32+ BaseAdd;BaseAdd=0*/
      /*由于担心8位单片机有乘法溢出问题，所以分三部取地址*/
      fontaddr = (text[i]- 0xa1)*94;
      fontaddr += (text[i+1]-0xa1);
      fontaddr = (ulong)(fontaddr*32);

      addr.high = (fontaddr&0xff0000)>>16;  /*地址的高8位,共24位*/
      addr.mid = (fontaddr&0xff00)>>8;      /*地址的中8位,共24位*/
      addr.low = fontaddr&0xff;       /*地址的低8位,共24位*/
      if(x + 16 > 128) {
        //new line
        x = 0;
        y += 16;
        if(y > 63) {
          break;
        }
      }
      //get_n_bytes_data_from_ROM(addrHigh,addrMid,addrLow,fontbuf,32 );/*取32个字节的数据，存到"fontbuf[32]"*/
      //display_graphic_16x16(y,x,fontbuf);/*显示汉字到LCD上，y为页地址，x为列地址，fontbuf[]为数据*/
      spi_get_data_tobuf(spifont, addr, fontbuf);
      ESP_LOGI(TAG,"draw_gb_string zone 2  Current index:%d ==  CHAR: %x%x ,pos[%d,%d]",i,text[i],text[i+1],x,y);
      printf("CHAR: ");
      for (int i = 0; i < strlen(text); ++i)
      {
        /* code */
        printf("%x", text[i]);
      }
      printf("\n");
      drawBitmap_togram(x, y, 16, 16, fontbuf);
      i+=2;
      x+=16;
    }
    else if((text[i]>=0x20) &&(text[i]<=0x7f))
    {
      // unsigned char fontbuf[16];
      fontaddr = (text[i]- 0x20);
      fontaddr = (unsigned long)(fontaddr*34);//16 for nomarl ,34 for arial
      fontaddr = (unsigned long)(fontaddr + 0x3d580);//0x3cf80 bold , 0x3b7c0 regular  0x3d580 for arial
      addr.high = (fontaddr&0xff0000)>>16;
      addr.mid = (fontaddr&0xff00)>>8;
      addr.low = fontaddr&0xff;
      //get_n_bytes_data_from_ROM(addrHigh,addrMid,addrLow,fontbuf,16 );/*取16个字节的数据，存到"fontbuf[32]"*/
      //display_graphic_8x16(y,x,fontbuf);/*显示8x16的ASCII字到LCD上，y为页地址，x为列地址，fontbuf[]为数据*/
      spi_get_data_tobuf_34(spifont, addr, fontbuf);
      ESP_LOGI(TAG,"draw_gb_string zone 3 Current index:%d ==  CHAR: %x ,pos[%d,%d] , width:%d",i,text[i],x,y,fontbuf[1]);
      printf("CHAR: ");
      for (int i = 0; i < strlen(text); ++i)
      {
        /* code */
        printf("%x", text[i]);
      }
      printf("\n");
      if(x + fontbuf[1]> 128) {
        //new line
        x = 0;
        y += 16;
        if(y > 63) {
          break;
        }
      }
      drawBitmap_togram(x, y, fontbuf[1], 16, fontbuf+2);
      i+=1;
      x+=fontbuf[1];
    }
    else
      i++;
  }
  refreshex(false);
}

void spi_get_data_tobuf(spi_device_handle_t spi, fontaddr_t addr,
                        uint8_t *buff) {
  // uint8_t fontbuf[32];
  // get_id cmd
  // Reset the display
  gpio_set_level(PIN_FONT_CS, 0);
  gpio_set_level(PIN_OLED_CS, 1);

  spi_cmd(spi, 0x03);

  spi_cmd(spi, addr.high);
  spi_cmd(spi, addr.mid);
  spi_cmd(spi, addr.low);

  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = 8 * 32;
  // t.flags = SPI_TRANS_USE_RXDATA;
  t.user = (void *)1;
  t.rx_buffer = buff;

  esp_err_t ret = spi_device_transmit(spi, &t);
  assert(ret == ESP_OK);

  gpio_set_level(PIN_FONT_CS, 1);
  gpio_set_level(PIN_OLED_CS, 0);
}

void select_font(uint8_t idx) {
  if (idx < NUM_FONTS) {
    font = fonts[idx];
  }
}

void spi_get_data_tobuf_34(spi_device_handle_t spi, fontaddr_t addr,
                        uint8_t *buff) {
  // uint8_t fontbuf[32];
  // get_id cmd
  // Reset the display
  gpio_set_level(PIN_FONT_CS, 0);
  gpio_set_level(PIN_OLED_CS, 1);

  spi_cmd(spi, 0x03);

  spi_cmd(spi, addr.high);
  spi_cmd(spi, addr.mid);
  spi_cmd(spi, addr.low);

  spi_transaction_t t;
  //read first 2 byte
  memset(&t, 0, sizeof(t));
  t.length = 8 * 2;
  // t.flags = SPI_TRANS_USE_RXDATA;
  t.user = (void *)1;
  t.rx_buffer = buff;

  esp_err_t ret = spi_device_transmit(spi, &t);
  assert(ret == ESP_OK);

  //then next 32 byte
  memset(&t, 0, sizeof(t));
  t.length = 8 * 32;
  // t.flags = SPI_TRANS_USE_RXDATA;
  t.user = (void *)1;
  t.rx_buffer = buff + 2;

  ret = spi_device_transmit(spi, &t);
  assert(ret == ESP_OK);


  gpio_set_level(PIN_FONT_CS, 1);
  gpio_set_level(PIN_OLED_CS, 0);
}

void drawBitmap_togram(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                       const uint8_t *buf) {
  uint8_t i, j, p;
  // printf("write region before:%d,%d -
  // %d,%d\n",refresh_left,refresh_top,refresh_right,refresh_bottom );
  // uint16_t cnt = 0;
  uint8_t remainder = (128 - x) < w ? (w + x - 128) : 0;

  w -= remainder;
  uint8_t remainder_y = (64 - y) < h ? (h + y - 64) : 0;
  h -= remainder_y;
  p = (y + h - 1) >> 3;
  // if (p > 7)
  // {
  //      code
  //     p = 7;
  // }
  for (j = (y >> 3); j <= p; j++) {
    for (i = x; i < x + w; i++) {
      // ssd1306_lcd.send_pixels1(s_ssd1306_invertByte^pgm_read_byte(buf++));
      gbuf[(j << 7) + i] = pgm_read_byte(buf++);
    }
    buf += (remainder + (16 - w));
    // ssd1306_lcd.next_page();
  }
  // ssd1306_intf.stop();
  // update refresh region

  if (refresh_left > x) {
    refresh_left = x;
  }

  if (refresh_top > y) {
    refresh_top = y;
  }

  if (refresh_right < x || refresh_right < x + w - 1) {
    refresh_right = x + w - 1;
  }
  //  else {
  //   if ((x + w - 1) > refresh_right) {
  //     refresh_right = x + w - 1;
  //   }
  // }

  if (refresh_bottom < y || refresh_bottom < y + h - 1) {
    refresh_bottom = y + h - 1;
  }
  // else {
  //   if ((y + h - 1) > refresh_bottom) {
  //     refresh_bottom = y + h - 1;
  //   }
  // }
  // printf("%d = %d + %d\n", refresh_bottom,y,h);
  // printf("%d,%d-%d,%d\n",x,y,w,h );
  // printf("write region :%d,%d -
  // %d,%d\n",refresh_left,refresh_top,refresh_right,refresh_bottom );
}

// return character width
uint8_t draw_char(uint8_t x, uint8_t y, unsigned char c) {
  const uint8_t *bitmap;

  if (font == NULL) {
    return 0;
  }

  // we always have space in the font set
  if ((c < font->char_start) || (c > font->char_end)) {
    c = ' ';
  }
  c = c - font->char_start;  // c now become index to tables
  bitmap = font->bitmap + font->char_descriptors[c].offset;
  drawBitmap_togram(x, y, font->char_descriptors[c].width, 16, bitmap);
  // ssd1306_drawBufferFast(x,y,font->char_descriptors[c].width,16, bitmap);

  return (font->char_descriptors[c].width);
}

uint8_t draw_string(uint8_t x, uint8_t y, char *str) {
  uint8_t t = x;

  if (font == NULL) {
    return 0;
  }

  uint8_t len = strlen(str);
  if (len == 0) {
    return 0;
  }

  for (int i = 0; i < len; i++) {
    x += draw_char(x, y, str[i]);
    if (str[i] != '\0') {
      x += font->c;
    }
  }

  return (x - t);
}

void cleargbuf() {
  for (int i = 0; i < 1024; ++i) {
    /* code */
    gbuf[i] = 0x00;
  }
}

/**
 * @param force 1 refresh all screen , 0 for region of screen
 */
void refreshex(uint8_t force) {
  // uint8_t i, j;
  // uint16_t k;
  // uint8_t page_start, page_end;
  if (force) {
    ssd1306_drawBufferFast(0, 0, 128, 64, gbuf);
    cleargbuf();
    // command(address, 0x21); // SSD1306_COLUMNADDR
    // command(address, 0);    // column start
    // command(address, 127);  // column end
    // command(address, 0x22); // SSD1306_PAGEADDR
    // command(address, 0);    // page start
    // command(address, 7);    // page end (8 pages for 64 rows OLED)
    // for (k = 0; k < 1024; k++) {
    //     i2c->start();
    //     i2c->write(address);
    //     i2c->write(0x40);
    //     for (j = 0; j < 16; ++j) {
    //         i2c->write(buffer[k]);
    //         ++k;
    //     }
    //     --k;
    //     i2c->stop();
    // }
  } else {
    if ((refresh_top <= refresh_bottom) && (refresh_left <= refresh_right)) {
      uint16_t cnt = 0;
      for (uint8_t j = (refresh_top >> 3); j <= (refresh_bottom >> 3); ++j) {
        for (uint8_t i = refresh_left; i <= refresh_right; ++i) {
          vbuf[cnt++] = gbuf[(j << 7) + i];  // j*128 + i
          // reset the refresh pixal
          gbuf[(j << 7) + i] = 0;  // j*128 + i
        }
      }
      ssd1306_drawBufferFast(refresh_left, refresh_top,
                             refresh_right - refresh_left + 1,
                             refresh_bottom - refresh_top + 1, vbuf);
      // page_start = refresh_top / 8;
      // page_end = refresh_bottom / 8;
      // command(address, 0x21); // SSD1306_COLUMNADDR
      // command(address, refresh_left);    // column start
      // command(address, refresh_right);   // column end
      // command(address, 0x22); // SSD1306_PAGEADDR
      // command(address, page_start);    // page start
      // command(address, page_end); // page end
      // k = 0;
      // for (i = page_start; i <= page_end; ++i) {
      //     for (j = refresh_left; j <= refresh_right; ++j) {
      //         if (k == 0) {
      //             i2c->start();
      //             i2c->write(address);
      //             i2c->write(0x40);
      //         }
      //         i2c->write(buffer[i * width + j]);
      //         ++k;
      //         if (k == 16) {
      //             i2c->stop();
      //             k = 0;
      //         }

      //     }
      // }
      // if (k != 0) // for last batch if stop was not sent
      //     i2c->stop();
    }
  }
  // printf("Refresh region :%d,%d -
  // %d,%d\n",refresh_left,refresh_top,refresh_right,refresh_bottom );
  // reset dirty area
  refresh_top = 255;
  refresh_left = 255;
  refresh_right = 0;
  refresh_bottom = 0;
}

