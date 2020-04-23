/* RMT example -- RGB LED Strip

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "sdkconfig.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "driver/rmt.h"
#include "led_strip.h"

static const char *TAG = "flickerLEDdemo";


//*****************************************************************
// - Config Tests:
// - Uncomment nescessary defines
#define WIFI_ACTIVE  //Activate WIFI, should lead to occasional flickering
#define MORELEDSTRIPES_ACTIVE  //Activate 5 more Stripes (for testing)
//#define MAXLEDSTRIPE_ACTIVE  //Activate RMTModule 6-7, so all RMT Channels are Running. Does not work. crash.
//*****************************************************************
//Set Amount of LEDs per Stripe.
//It only starts to show the flickering with 44 LEDs and more. (multiple rmt memory blocks)
#define CONFIG_EXAMPLE_STRIP_LED_NUMBER 300  
//*****************************************************************

#define EXAMPLE_ESP_WIFI_SSID      "yourssid"
#define EXAMPLE_ESP_WIFI_PASS      "yourpassword"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5


#define PIN_LEDPOWER    33
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#define CONFIG_EXAMPLE_RMT_TX_GPIO 13

#ifdef MORELEDSTRIPES_ACTIVE
    #define LED2_RMTCHANNEL 1
    #define LED3_RMTCHANNEL	2
    #define LED4_RMTCHANNEL	3
    #define LED5_RMTCHANNEL	4
    #define LED6_RMTCHANNEL	5
    #define LED2_RMT_TX_GPIO 15
    #define LED3_RMT_TX_GPIO 17
    #define LED4_RMT_TX_GPIO 16
    #define LED5_RMT_TX_GPIO 04
    #define LED6_RMT_TX_GPIO 02
#endif

#ifdef MAXLEDSTRIPE_ACTIVE
    #define LED7_RMTCHANNEL	6
    #define LED8_RMTCHANNEL	7
    #define LED7_RMT_TX_GPIO 19
    #define LED8_RMT_TX_GPIO 18
#endif

#ifdef WIFI_ACTIVE
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}
#endif

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

void app_main(void)
{
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    uint16_t hue = 0;
    uint16_t start_rgb = 0;

    ESP_LOGI(TAG, "[APP] WS2812-Flicker-Problem-Demo");
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

#ifdef WIFI_ACTIVE
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    ESP_LOGI(TAG, "[APP] Free memory when WIFI initialized: %d bytes", esp_get_free_heap_size());
#endif

    gpio_set_direction(PIN_LEDPOWER, GPIO_MODE_OUTPUT); //nescessary for my setup. nothing what would interfere with the test
    gpio_set_level(PIN_LEDPOWER, 1);

    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(CONFIG_EXAMPLE_RMT_TX_GPIO, RMT_TX_CHANNEL);
    // set counter clock to 40MHz
    config.clk_div = 2;

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    // install ws2812 driver
    led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(CONFIG_EXAMPLE_STRIP_LED_NUMBER, (led_strip_dev_t)config.channel);
    led_strip_t *strip = led_strip_new_rmt_ws2812(&strip_config);
    if (!strip) {
        ESP_LOGE(TAG, "install WS2812 driver failed");
    }
    ESP_LOGI(TAG, "[APP] Free memory when Stripe1 initialized: %d bytes", esp_get_free_heap_size());

#ifdef MORELEDSTRIPES_ACTIVE
    rmt_config_t config_2 = RMT_DEFAULT_CONFIG_TX(LED2_RMT_TX_GPIO, LED2_RMTCHANNEL);
    rmt_config_t config_3 = RMT_DEFAULT_CONFIG_TX(LED3_RMT_TX_GPIO, LED3_RMTCHANNEL);
    rmt_config_t config_4 = RMT_DEFAULT_CONFIG_TX(LED4_RMT_TX_GPIO, LED4_RMTCHANNEL);
    rmt_config_t config_5 = RMT_DEFAULT_CONFIG_TX(LED5_RMT_TX_GPIO, LED5_RMTCHANNEL);
    rmt_config_t config_6 = RMT_DEFAULT_CONFIG_TX(LED6_RMT_TX_GPIO, LED6_RMTCHANNEL);
    // set counter clock to 40MHz
    config_2.clk_div = 2;
    config_3.clk_div = 2;
    config_4.clk_div = 2;
    config_5.clk_div = 2;
    config_6.clk_div = 2;

    ESP_ERROR_CHECK(rmt_config(&config_2));
    ESP_ERROR_CHECK(rmt_config(&config_3));
    ESP_ERROR_CHECK(rmt_config(&config_4));
    ESP_ERROR_CHECK(rmt_config(&config_5));
    ESP_ERROR_CHECK(rmt_config(&config_6));
    ESP_ERROR_CHECK(rmt_driver_install(config_2.channel, 0, 0));
    ESP_ERROR_CHECK(rmt_driver_install(config_3.channel, 0, 0));
    ESP_ERROR_CHECK(rmt_driver_install(config_4.channel, 0, 0));
    ESP_ERROR_CHECK(rmt_driver_install(config_5.channel, 0, 0));
    ESP_ERROR_CHECK(rmt_driver_install(config_6.channel, 0, 0));

    // install ws2812 driver
    led_strip_config_t strip_config_2 = LED_STRIP_DEFAULT_CONFIG(CONFIG_EXAMPLE_STRIP_LED_NUMBER, (led_strip_dev_t)config_2.channel);
    led_strip_config_t strip_config_3 = LED_STRIP_DEFAULT_CONFIG(CONFIG_EXAMPLE_STRIP_LED_NUMBER, (led_strip_dev_t)config_3.channel);
    led_strip_config_t strip_config_4 = LED_STRIP_DEFAULT_CONFIG(CONFIG_EXAMPLE_STRIP_LED_NUMBER, (led_strip_dev_t)config_4.channel);
    led_strip_config_t strip_config_5 = LED_STRIP_DEFAULT_CONFIG(CONFIG_EXAMPLE_STRIP_LED_NUMBER, (led_strip_dev_t)config_5.channel);
    led_strip_config_t strip_config_6 = LED_STRIP_DEFAULT_CONFIG(CONFIG_EXAMPLE_STRIP_LED_NUMBER, (led_strip_dev_t)config_6.channel);
    led_strip_t *strip_2 = led_strip_new_rmt_ws2812(&strip_config_2);
    led_strip_t *strip_3 = led_strip_new_rmt_ws2812(&strip_config_3);
    led_strip_t *strip_4 = led_strip_new_rmt_ws2812(&strip_config_4);
    led_strip_t *strip_5 = led_strip_new_rmt_ws2812(&strip_config_5);
    led_strip_t *strip_6 = led_strip_new_rmt_ws2812(&strip_config_6);
    if (!strip_2) {
        ESP_LOGE(TAG, "install WS2812 driver failed");
    }
    if (!strip_3) {
        ESP_LOGE(TAG, "install WS2812 driver failed strip 3");
    }
    if (!strip_4) {
        ESP_LOGE(TAG, "install WS2812 driver failed strip 4");
    }
    if (!strip_5) {
        ESP_LOGE(TAG, "install WS2812 driver failed strip 5");
    }
    if (!strip_6) {
        ESP_LOGE(TAG, "install WS2812 driver failed strip 6");
    }
    ESP_LOGI(TAG, "[APP] Free memory when Stripe 2-6 initialized: %d bytes", esp_get_free_heap_size());
#endif

#ifdef MAXLEDSTRIPE_ACTIVE
    rmt_config_t config_7 = RMT_DEFAULT_CONFIG_TX(LED7_RMT_TX_GPIO, LED7_RMTCHANNEL);
    rmt_config_t config_8 = RMT_DEFAULT_CONFIG_TX(LED8_RMT_TX_GPIO, LED8_RMTCHANNEL);
    // set counter clock to 40MHz
    config_7.clk_div = 2;
    config_8.clk_div = 2;

    
    ESP_ERROR_CHECK(rmt_config(&config_7));
    ESP_ERROR_CHECK(rmt_config(&config_8));
    
    ESP_ERROR_CHECK(rmt_driver_install(config_7.channel, 0, 0));
    ESP_ERROR_CHECK(rmt_driver_install(config_8.channel, 0, 0));

    // install ws2812 driver
    
    led_strip_config_t strip_config_7 = LED_STRIP_DEFAULT_CONFIG(CONFIG_EXAMPLE_STRIP_LED_NUMBER, (led_strip_dev_t)config_7.channel);
    led_strip_config_t strip_config_8 = LED_STRIP_DEFAULT_CONFIG(CONFIG_EXAMPLE_STRIP_LED_NUMBER, (led_strip_dev_t)config_8.channel);
    
    led_strip_t *strip_7 = led_strip_new_rmt_ws2812(&strip_config_7);
    led_strip_t *strip_8 = led_strip_new_rmt_ws2812(&strip_config_8);
    
    if (!strip_7) {
        ESP_LOGE(TAG, "install WS2812 driver failed strip 7");
    }
    if (!strip_8) {
        ESP_LOGE(TAG, "install WS2812 driver failed strip 8");
    }
    ESP_LOGI(TAG, "[APP] Free memory when Stripe3 7-8 initialized: %d bytes", esp_get_free_heap_size());
#endif

    // Clear LED strip (turn off all LEDs)
    ESP_ERROR_CHECK(strip->clear(strip, 100));

#ifdef MORELEDSTRIPES_ACTIVE
    // Clear LED strip (turn off all LEDs)
    ESP_ERROR_CHECK(strip_2->clear(strip_2, 100));
    ESP_ERROR_CHECK(strip_3->clear(strip_3, 100));
    ESP_ERROR_CHECK(strip_4->clear(strip_4, 100));
    ESP_ERROR_CHECK(strip_5->clear(strip_5, 100));
    ESP_ERROR_CHECK(strip_6->clear(strip_6, 100));
#endif

#ifdef MAXLEDSTRIPE_ACTIVE
    // Clear LED strip (turn off all LEDs)
    ESP_ERROR_CHECK(strip_7->clear(strip_7, 100));
    ESP_ERROR_CHECK(strip_8->clear(strip_8, 100));
#endif


    // Show simple rainbow chasing pattern
    ESP_LOGI(TAG, "LED Test Start");
    while (true) {
        if(++hue == 360) {
            hue=0;
        }
        led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
        for(int i = 0; i < CONFIG_EXAMPLE_STRIP_LED_NUMBER;i++) {
            if(i > CONFIG_EXAMPLE_STRIP_LED_NUMBER/2) {
                strip->set_pixel(strip, i, red, green, blue);
            } else {
                strip->set_pixel(strip, i, 0x80, 0x00, 0x00);
            }
        }
        strip->refresh(strip, 10);

#ifdef MORELEDSTRIPES_ACTIVE
        for(int i = 0; i < CONFIG_EXAMPLE_STRIP_LED_NUMBER;i++) {
            if(i > CONFIG_EXAMPLE_STRIP_LED_NUMBER/2) {
                strip_2->set_pixel(strip_2, i, red, green, blue);
                strip_3->set_pixel(strip_3, i, red, green, blue);
                strip_4->set_pixel(strip_4, i, red, green, blue);
                strip_5->set_pixel(strip_5, i, red, green, blue);
                strip_6->set_pixel(strip_6, i, red, green, blue);
            } else {
                strip_2->set_pixel(strip_2, i, 0x80, 0x00, 0x00);
                strip_3->set_pixel(strip_3, i, 0x80, 0x00, 0x00);
                strip_4->set_pixel(strip_4, i, 0x80, 0x00, 0x00);
                strip_5->set_pixel(strip_5, i, 0x80, 0x00, 0x00);
                strip_6->set_pixel(strip_6, i, 0x80, 0x00, 0x00);
            }
        }
        strip_2->refresh(strip_2, 10);
        strip_3->refresh(strip_3, 10);
        strip_4->refresh(strip_4, 10);
        strip_5->refresh(strip_5, 10);
        strip_6->refresh(strip_6, 10);
#endif

#ifdef MAXLEDSTRIPE_ACTIVE
        for(int i = 0; i < CONFIG_EXAMPLE_STRIP_LED_NUMBER;i++) {
            if(i > CONFIG_EXAMPLE_STRIP_LED_NUMBER/2) {
                strip_7->set_pixel(strip_7, i, red, green, blue);
                strip_8->set_pixel(strip_8, i, red, green, blue);
            } else {
                strip_7->set_pixel(strip_7, i, 0x80, 0x00, 0x00);
                strip_8->set_pixel(strip_8, i, 0x80, 0x00, 0x00);
            }
        }
        strip_7->refresh(strip_7, 10);
        strip_8->refresh(strip_8, 10);
#endif
        
        vTaskDelay(10);
    }
}
