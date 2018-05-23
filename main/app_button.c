// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          user_button.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-02-22  AWe   adept for use with ESP32/ESP-IDF
//    2017-11-25  AWe   move button related functions to  user_button.c
//    2017-11-15  AWe   do WPS on long press, relay on/off on short press ( not implemented yet )
//                      add WPS related functions resetBtnTimerCb(),
//                        ioBtnTimerInit() from io.c
//    2017-08-19  AWe   change debug message printing
//    2017-08-07  AWe   initial implementation
//                      take over some wps stuff from
//                        ESP8266_Arduino\libraries\ESP8266WiFi\src\ESP8266WiFiSTA.cpp
//
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_INFO
static const char *TAG = "app_button";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"       // vTaskDelay()

#include "keys.h"
#include "leds.h"
#include "app_wifi.h"

// --------------------------------------------------------------------------
// app_wifi.c

void wifiStartWps( void ) ;

// --------------------------------------------------------------------------
// configure pins
// --------------------------------------------------------------------------

#include "sdkconfig.h"

#ifdef CONFIG_USE_WPS_BTN
   #define WPS_BTN_GPIO       CONFIG_WPS_BTN_GPIO
#else
   #error pin for WPS button not defined
#endif

#define BUTTON_NUM        1
#define WPS_BUTTON        0

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static struct keys_param keys;
static struct single_key_param *single_key_list[ BUTTON_NUM ];

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void user_button_short_press( void )
{
   ESP_LOGI( TAG, "user_button_short_press" );

   // !!! todo ( AWe ): do something on short press
   wifiShowStatus();
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void user_button_long_press( void )
{
   ESP_LOGI( TAG, "user_button_long_press" );

   // !!! todo ( AWe ): do something on long press
   // start wps task
   wifiStartWps();
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void user_button_very_long_press( void )
{
   ESP_LOGI( TAG, "user_button_very_long_press" );

   // !!! todo ( AWe ): do something on long press
   // restart device
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void appButtonTask( void *pvParameter )
{
   ESP_LOGD( TAG, "appButtonTask" );

   single_key_list[ WPS_BUTTON ] = single_key_init( WPS_BUTTON, WPS_BTN_GPIO,
                                user_button_very_long_press, 10000,
                                user_button_long_press, 2000,
                                user_button_short_press );

   keys_init( &keys, BUTTON_NUM, single_key_list );

   while( true )
   {
      keys_process( &keys );
      // ESP_LOGD( TAG, "GPIO.status 0x%08x", GPIO.status );

      vTaskDelay( 50 / portTICK_RATE_MS );  // debounce time 50ms
   }

   keys_deinit( &keys );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void appLedTask( void *pvParameter )
{
   led_set( SYS_LED, LED_BLINK, 5, 5, 0 );     // 500ms on, 500ms off

   while( 1 )
   {
      vTaskDelay( 100 / portTICK_PERIOD_MS );
      leds_update();
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
