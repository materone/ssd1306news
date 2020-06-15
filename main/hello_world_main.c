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
// #include "esp_event_loop.h"
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

#include "lwip/apps/sntp.h"
// #include "lwip/dns.h"

// network
#include "lwip/err.h"
#include "lwip/sys.h"
#include "ssd1306_1bit.h"

#include "fonts.h"
#include "ssd1306enc.h"
#include "timer_fi.h"

#include "cJSON.h"

//include pass and dnsgw
#include "p.h"
//p.h like 
// static const char STAS[][32] = {"AA", "BB"};
// static const char PASSS[][64] = {"11", "22"};
// static uint32_t DNSGW[] = {ESP_IP4TOADDR( 192,168,100,3),ESP_IP4TOADDR( 192,168,100,4)}

#include "esp_http_client.h"
#define MAX_HTTP_RECV_BUFFER 512


/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;
const int SCANDONE_BIT = BIT1;

static const char *TAG = "OLED-WiFi-News";


int staidx = 0;
spi_device_handle_t spifont;


#define EXAMPLE_ESP_MAXIMUM_RETRY  5

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_scan(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    uint16_t apCount = 0;
    bool flag = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&apCount));
    ESP_LOGI(TAG, "Total APs scanned = %u", apCount);
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
      if(!flag)
      {
        for (int j = 0; j < sizeof(STAS) / sizeof(STAS[0]); ++j) {
              if (strcmp((const char *)(list[i].ssid), STAS[j]) == 0) {
                flag = true;
                staidx = j;
                break;
              }
            }
        }
    }

    if (flag) {
      ESP_LOGI(TAG, "Find Config AP %s", STAS[staidx]);
    }
    free(list);
    ESP_ERROR_CHECK(esp_wifi_scan_stop());
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    // ESP_ERROR_CHECK(esp_netif_deinit());
    return ESP_OK;
}
void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t* esp_handle = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    wifi_config_t wifi_config = {
        .sta = {
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    memcpy(wifi_config.sta.ssid, STAS[staidx], sizeof(uint8_t) * 32);
    memcpy(wifi_config.sta.password, PASSS[staidx], sizeof(uint8_t) * 64);    
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    //set ip
    esp_netif_ip_info_t ip_info = {
        .ip = { .addr = ESP_IP4TOADDR( 192,168,100,188) },
        .gw = { .addr = DNSGW[staidx] },
        .netmask = { .addr = ESP_IP4TOADDR( 255, 255, 255, 0) },
    };
    //set dns ip
    esp_netif_dns_info_t dns_info = {
        .ip.u_addr.ip4.addr = DNSGW[staidx],
    };
    //set dns
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(esp_handle));
    ESP_LOGI(TAG, "dhcp stop.");
    ESP_ERROR_CHECK(esp_wifi_connect());
    ESP_LOGI(TAG, "connect.");
    ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_handle, &ip_info));    
    ESP_ERROR_CHECK(esp_netif_set_dns_info(esp_handle, ESP_NETIF_DNS_MAIN, &dns_info));
    ESP_LOGI(TAG, "set ip .");
    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s",STAS[staidx]);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s",STAS[staidx]);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    // vEventGroupDelete(wifi_event_group);
}
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
    // xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, pdFALSE, pdTRUE,
    //                   portMAX_DELAY);
    xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);
    esp_http_client_config_t config = {
        .url = "http://jira.find73.com:12350/news",
        .event_handler = _http_event_handler,
    };
    ESP_LOGI(TAG, "Connected and will get news");
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
  char *msg,*logo;
  portTickType xLastWakeTime;
  const portTickType xFrequency = 10000 / portTICK_PERIOD_MS;
  uint8_t loopcnt = 0;
  for(;;){
    loopcnt++;
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
    char *space = " ";
    char *no = NULL;
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
      // ESP_LOGI(TAG,"LEN:%d,VAL:%s",strlen(no),no);
      logo = calloc(strlen(js_type->valuestring) + 1 + strlen(js_date->valuestring) + 1,1);
      logo = strcat(logo,js_type->valuestring);
      logo = strcat(logo,space);
      logo = strcat(logo,js_date->valuestring);
      logo = strcat(logo,space);
      no = calloc(7+strlen(js_title->valuestring)+1,1);//000-000+val
      sprintf(no,"%03d-%03d",loopcnt,i);
      msg = strcat(no,js_title->valuestring);
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
      if(no){
        free(no);
      }
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
  //scan wifi
  ESP_ERROR_CHECK(wifi_scan());
  //initial wifi
  wifi_init_sta();
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

  xTaskCreate(&lcdwrite, "lcdwrite", 8192, NULL, 5, NULL);

}
