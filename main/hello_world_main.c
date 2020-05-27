/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
#include <stdio.h>
#include <stdlib.h>
// #include <iconv.h>
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "imgbmp.h"
#include "ssd1306.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"

#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

// for sntp
#include <sys/time.h>
#include <time.h>
#include "esp_attr.h"
#include "esp_log.h"
// #include "esp_deep_sleep.h"
#include "driver/timer.h"

#include "apps/sntp/sntp.h"
#include "lwip/dns.h"
#include "lwip/err.h"

#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "ssd1306_1bit.h"

#include "fonts.h"
#include "ssd1306enc.h"
#include "timer_fi.h"

#include "cJSON.h"


#include "esp_http_client.h"
#define MAX_HTTP_RECV_BUFFER 512

/* FreeRTOS event group to signal when we are connected & ready to make a
 * request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;
const int SCANDONE_BIT = BIT1;

static const char *TAG = "OLED-WiFi-RTC";

static const char STAS[][32] = {"AAA", "BBB"};
static const char PASSS[][64] = {"XXX", "XXX"};
int staidx = 0;
spi_device_handle_t spifont;

uint8_t jiong1[] = {/*--  文字:  囧  --*/
                    /*--  宋体12;  此字体下对应的点阵为：宽x高=16x16   --*/
  0x00, 0xFE, 0x82, 0x42, 0xA2, 0x9E, 0x8A, 0x82,
  0x86, 0x8A, 0xB2, 0x62, 0x02, 0xFE, 0x00, 0x00,
  0x00, 0x7F, 0x40, 0x40, 0x7F, 0x40, 0x40, 0x40,
  0x40, 0x40, 0x7F, 0x40, 0x40, 0x7F, 0x00, 0x00
};

uint8_t lei1[] = {/*--  文字:  畾  --*/
                  /*--  宋体12;  此字体下对应的点阵为：宽x高=16x16   --*/
  0x80, 0x80, 0x80, 0xBF, 0xA5, 0xA5, 0xA5, 0x3F,
  0xA5, 0xA5, 0xA5, 0xBF, 0x80, 0x80, 0x80, 0x00,
  0x7F, 0x24, 0x24, 0x3F, 0x24, 0x24, 0x7F, 0x00,
  0x7F, 0x24, 0x24, 0x3F, 0x24, 0x24, 0x7F, 0x00
};

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Write out data
                // printf("%.*s", evt->data_len, (char*)evt->data);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

// Send a command to the LCD. Uses spi_device_transmit, which waits until the
// transfer is complete.

esp_err_t event_handler_scan(void *ctx, system_event_t *event) {
  // ostringstream os;
  if (event->event_id == SYSTEM_EVENT_SCAN_DONE) {
    uint16_t apCount = 0;
    bool flag = false;
    esp_wifi_scan_get_ap_num(&apCount);
    printf("Number of access points found: %d\n",
           event->event_info.scan_done.number);
    if (apCount == 0) {
      return ESP_OK;
    }
    wifi_ap_record_t *list =
      (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, list));
    int i;
    printf(
      "======================================================================"
      "\n");
    printf(
      "             SSID             |    RSSI    |           AUTH           "
      "\n");
    printf(
      "======================================================================"
      "\n");
    for (i = 0; i < apCount; i++) {
      char *authmode;
      switch (list[i].authmode) {
      case WIFI_AUTH_OPEN:
        authmode = (char *)"WIFI_AUTH_OPEN";
        break;
      case WIFI_AUTH_WEP:
        authmode = (char *)"WIFI_AUTH_WEP";
        break;
      case WIFI_AUTH_WPA_PSK:
        authmode = (char *)"WIFI_AUTH_WPA_PSK";
        break;
      case WIFI_AUTH_WPA2_PSK:
        authmode = (char *)"WIFI_AUTH_WPA2_PSK";
        break;
      case WIFI_AUTH_WPA_WPA2_PSK:
        authmode = (char *)"WIFI_AUTH_WPA_WPA2_PSK";
        break;
      default:
        authmode = (char *)"Unknown";
        break;
      }
      printf("%26.26s    |    % 4d    |    %22.22s\n", list[i].ssid,
             list[i].rssi, authmode);
      for (int j = 0; j < sizeof(STAS) / sizeof(STAS[0]); ++j) {
        if (strcmp((const char *)(list[i].ssid), STAS[j]) == 0) {
          flag = true;
          staidx = j;
          break;
        }
      }
    }

    if (flag) {
      ESP_LOGI(TAG, "Find Config AP %s", STAS[staidx]);
    }
    free(list);
    xEventGroupSetBits(wifi_event_group, SCANDONE_BIT);
  }
  return ESP_OK;
}

static esp_err_t event_handler(void *ctx, system_event_t *event) {
  switch (event->event_id) {
  case SYSTEM_EVENT_STA_START:
    esp_wifi_connect();
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    xEventGroupClearBits(wifi_event_group, SCANDONE_BIT);
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    /* This is a workaround as ESP32 WiFi libs don't currently
       auto-reassociate. */
    esp_wifi_connect();
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    break;
  default:
    break;
  }
  return ESP_OK;
}

static void initialize_sntp(void) {
  ESP_LOGI(TAG, "Initializing SNTP");
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(
    0, (char *)"0.pool.ntp.org");    // 45.76.98.188 pool.ntp.org 192.168.78.51
  sntp_setservername(
    1, (char *)"1.pool.ntp.org");    // 45.76.98.188 pool.ntp.org 192.168.78.51
  sntp_setservername(
    2, (char *)"2.pool.ntp.org");    // 45.76.98.188 pool.ntp.org 192.168.78.51
  sntp_setservername(
    3, (char *)"3.pool.ntp.org");    // 45.76.98.188 pool.ntp.org 192.168.78.51
  sntp_init();
}

static void initialise_wifi(void) {
  tcpip_adapter_init();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  wifi_config_t wifi_config;
  // wifi_config.sta.ssid = STAS[staidx];
  // wifi_config.sta.password = PASSS[staidx];
  memcpy(wifi_config.sta.ssid, STAS[staidx], sizeof(uint8_t) * 32);
  memcpy(wifi_config.sta.password, PASSS[staidx], sizeof(uint8_t) * 64);
  wifi_config.sta.bssid_set = false;

  ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
  // oled.draw_string(55, 41, (char *)wifi_config.sta.ssid, WHITE, BLACK);
  // oled.refresh(false);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  tcpip_adapter_ip_info_t ip_info;
  ip4_addr_t ip = {ipaddr_addr("192.168.199.123")};
  ip4_addr_t netmask = {ipaddr_addr("255.255.255.0")};
  ip4_addr_t gw = {ipaddr_addr("192.168.199.1")};
  ip_info.ip = ip;
  ip_info.netmask = netmask;
  ip_info.gw = gw;

  tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
  ESP_ERROR_CHECK(esp_wifi_connect());
  tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
  // set dns
  ip_addr_t dns;
  ipaddr_aton("114.114.114.114",
              &dns);  // 8.8.8.8 192.168.70.21 114.114.114.114
  dns_setserver(0, &dns);
}

static void obtain_time(void) {
  // ESP_ERROR_CHECK( nvs_flash_init() );
  xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true,
                      portMAX_DELAY);
  initialize_sntp();

  // wait for time to be set
  time_t now = 0;
  struct tm timeinfo = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  int retry = 0;
  const int retry_count = 60;
  while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
    ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry,
             retry_count);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    time(&now);
    localtime_r(&now, &timeinfo);
  }

  ESP_ERROR_CHECK(esp_wifi_stop());
}

void clearmsg(char msg[]){
  for (int i = 0; i < 49; ++i)
  {
    /* code */
    msg[i] = 0x00;
  }
}

/**
 * Get news from chinanews, result will encode with json 
 * @author Tony
 * @DateTime 2019-01-06T11:09:59+0800
 * @return   [if error the NULL will return]
 */
static char* newsget()
{
    ESP_LOGI(TAG,"Begin get news");
    //wait wifi connected
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true,
                      portMAX_DELAY);
    esp_http_client_config_t config = {
        .url = "http://AAA.com:8080/news",
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err;
    if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        return NULL;
    }
    int content_length =  esp_http_client_fetch_headers(client);
    printf("content_length :%d\n",content_length );
    char *buffer = malloc(content_length + 1);
    buffer[content_length] = 0;
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Cannot malloc http receive buffer");
        return NULL;
    }
    int  read_len;//total_read_len = 0,
    if((read_len = esp_http_client_read(client, buffer, content_length)) != -1){
        // printf("%s\n", buffer);
        ESP_LOGD(TAG, "read_len = %d", read_len);
    }else{
        ESP_LOGE(TAG, "Cannot Get message from website");
    }
    ESP_LOGI(TAG, "HTTP Stream reader Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    //Free the buffer in caller
    //free(buffer);
    return buffer;
}

void lcdwrite() {
  // char *msg = "我们来ABC了新D一贷库这个 就是Za新的c计d划超平凡的力量就是";  //我们来了新一贷库
  char *msg,*logo;//= "[ 文化 ] 600亿与☆“创作暖春” -- 中国电影的2018";  //我们来了新一贷库
  portTickType xLastWakeTime;
  const portTickType xFrequency = 10000 / portTICK_PERIOD_MS;
  for(;;){
  //get msg from news;
    char *newslist = newsget();
    if(newslist == NULL){
      ESP_LOGE(TAG,"Cannot get news from website");
      vTaskDelete(NULL);
      return ;
    }
    //parse 
    cJSON *root = cJSON_Parse(newslist);
    if (!root)
    {
      /* code */
      ESP_LOGE(TAG,"parse json news error");
      free(newslist);
      return ;
    }

    cJSON *news = cJSON_GetObjectItem(root,"Data");
    if(!news){
      ESP_LOGE(TAG,"parse json news when get news error");
      free(newslist);
      cJSON_Delete(root);
      return ;
    }

    cJSON *item,*it,*js_type,*js_title,*js_date;
    int array_size = cJSON_GetArraySize(news);
    ESP_LOGI(TAG,"News Array size is %d",array_size);

    char *p = NULL;
    xLastWakeTime = xTaskGetTickCount();
    for (int i = 0; i < array_size; ++i)
    {
      
      ESP_LOGI(TAG,"%d === current free heap size %d" ,i,xPortGetFreeHeapSize());
      //clear screen block
      uint8_t *clrbuf = calloc(128*8,1);
      ssd1306_drawBufferFast(0,0,128,64,clrbuf);
      free(clrbuf);
      item = cJSON_GetArrayItem(news,i);
      if (!item)
      {
        /* code */
        ESP_LOGE(TAG,"parse json news error when get item from array");
        cJSON_Delete(root);
        return ;
      }

      p = cJSON_PrintUnformatted(item);
      it = cJSON_Parse(p);
      if (!it)
      {
        continue;
      }
      js_type = cJSON_GetObjectItem(it,"NewsType");
      if (!js_type)
      {
        continue;
      }
      js_title = cJSON_GetObjectItem(it,"Title");
      if (!js_title)
      {
        continue;
      }
      js_date = cJSON_GetObjectItem(it,"Date");
      if (!js_date)
      {
        continue;
      }
      char *space = " ";
      logo = calloc(strlen(js_type->valuestring) + 1 + strlen(js_date->valuestring) + 1,1);
      logo = strcat(logo,js_type->valuestring);
      logo = strcat(logo,space);
      logo = strcat(logo,js_date->valuestring);
      msg = js_title->valuestring;
      ESP_LOGI(TAG,"The String len :%d",strlen(msg));
      ESP_LOGI(TAG,"The News :%s",msg);
      draw_gb_string(0,0,logo);
      draw_gb_string(0,16,msg);
      //scroll first page 
      ssd1306_horizontal_scroll(0,1,4,1);
      ssd1306_active_scroll();
      vTaskDelayUntil( &xLastWakeTime, xFrequency );
      //deactive scroll
      ssd1306_deactive_scroll();
      if(p){
        free(p);
      }
      if(logo){
        free(logo);
      }
      if(it){
        cJSON_Delete(it);
      }
      // vTaskDelay(10000 / portTICK_PERIOD_MS);
    }

    free(newslist);
    if(root)
      cJSON_Delete(root);
  }
  
  // vTaskDelete(NULL);
}

void lcdwrite2() {
  char *msg = "我们来了新一贷库";
  char word[2];
  uint8_t iname[32];
  while (1) {
    int i = 0, x = 0, y = 2;
    while (msg[i] > 0x00) {
      word[0] = msg[i];
      word[1] = msg[i + 1];
      spi_get_data_tobuf(spifont, getStingAddr(word), iname);
      ssd1306_drawBitmap(x, y, 16, 16, iname);
      i += 2;
      x += 16;
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    ssd1306_drawBitmap(0, 0, 128, 64, bmp);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    ssd1306_clearScreen();
  }
}

void ntpc() {
  // ++boot_count;
  // ESP_LOGI(TAG, "Boot count: %d", boot_count);

  time_t now;
  uint8_t *cbuf;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  // Is time set? If not, tm_year will be (1970 - 1900).
  if (timeinfo.tm_year < (2016 - 1900)) {
    ESP_LOGI(
      TAG,
      "Time is not set yet. Connecting to WiFi and getting time over NTP.");
    obtain_time();
    // update 'now' variable with current time
    time(&now);
  }
  char strftime_buf[64];

  // Set timezone to Eastern Standard Time and print local time
  setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
  tzset();
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(TAG, "The current date/time in New York is: %s", strftime_buf);
  bool once = true;

  // Set timezone to China Standard Time
  setenv("TZ", "CST-8", 1);
  tzset();
  // print some text

  // lcdwrite();
  cbuf = calloc(128 * 2, 1);
  while (0) {
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c",
             &timeinfo);  //%c for common //%y%m%d %T
    if (once) {
      ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
      ssd1306_printFixed(0, 16, "Entering deep sleep FOUR", STYLE_NORMAL);
      // ssd1306_printFixed(0,32,"Some test info chars",STYLE_NORMAL);
      once = false;
    }

    
    ssd1306_drawBufferFast(0, 0, 128, 16, cbuf);

    draw_string(0, 0, strftime_buf);
    refreshex(false);
    // ssd1306_print(strftime_buf);
    // oled.draw_string(8,52,strftime_buf,WHITE,BLACK);
    // oled.refresh(false);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  free(cbuf);
}

void app_main() {
  printf("Hello world!\n");
  select_font(0);
  /* Print chip information */
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ", chip_info.cores,
         (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
         (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

  printf("silicon revision %d, ", chip_info.revision);

  printf(
    "%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
    (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

   fflush(stdout);

  nvs_flash_init();
  ssd1306_setFreeFont(free_calibri11x12);
  // ssd1306_128x64_i2c_init();
  ssd1306_128x64_spi_init(-1, 5, 21);  // Use this line for ESP32 (VSPI)
                                       // (gpio22=RST, gpio5=CE for VSPI,
                                       // gpio21=D/C)

  // ssd1306_fillScreen( 0x01);
  ssd1306_clearScreen();
  // ssd1306_charF12x16 (0,0,"Hi ESP32,Use this line for ESP32
  // (VSPI)!",STYLE_NORMAL);
  // test_timer();
  // wifi scan

  tcpip_adapter_init();

  wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_event_loop_init(event_handler_scan, NULL));
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  // Let us test a WiFi scan ...
  wifi_scan_config_t scanConf = {
    .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true
  };

  // start wifi scan
  ESP_ERROR_CHECK(esp_wifi_scan_start(
                    &scanConf,
                    true)); // The true parameter cause the function to block until
  // stop wifi scan
  // the scan is done.
  ESP_LOGI(TAG, "WiFi Scan Done!");
  xEventGroupWaitBits(wifi_event_group, SCANDONE_BIT, false, true,
                      portMAX_DELAY);
  ESP_LOGI(TAG, "WiFi Scan Done Bit set!");
  ESP_ERROR_CHECK(esp_wifi_scan_stop());
  // for wifi connect in the future
  esp_event_loop_set_cb(event_handler, NULL);

  initialise_wifi();

  // get font data

  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 10 * 1000 * 1000,    // Clock out at 10 MHz
    .mode = 0,                             // SPI mode 0
    .spics_io_num = PIN_FONT_CS,           // CS pin
    .queue_size = 7
  };
  esp_err_t ret;

  ret = spi_bus_add_device(VSPI_HOST, &devcfg, &spifont);
  ESP_ERROR_CHECK(ret);
  gpio_set_direction(PIN_FONT_CS, GPIO_MODE_OUTPUT);

  // lcdwrite();
  xTaskCreate(&lcdwrite, "lcdwrite", 8192, NULL, 5, NULL);
  // char *retnews = newsget();
  // free(retnews);
  // ntpc
  // ntpc();

  // vTaskDelay(10000 / portTICK_PERIOD_MS);

  // lcdwrite();

}
