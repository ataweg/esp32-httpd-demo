// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          leds.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-02-15  AWe   adept for use with ESP32 and ESP-IDF
//    2017-11-18  AWe
//
// --------------------------------------------------------------------------

// for pin definitions see
//   C:\Espressif\ESP8266_NONOS_SDK\include\eagle_soc.h

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_WARN
static const char *TAG = "leds";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <string.h>        // memset()
#include "driver/gpio.h"
#include "leds.h"


#ifdef ESP_PLATFORM
   #define ICACHE_FLASH_ATTR
   #define ICACHE_RODATA_ATTR
#endif

// --------------------------------------------------------------------------
// configure pins
// --------------------------------------------------------------------------

#include "sdkconfig.h"

#ifdef CONFIG_USE_SYS_LED
   #define SYS_LED_GPIO       CONFIG_SYS_LED_GPIO
#else
   #undef SYS_LED_GPIO
#endif

#ifdef CONFIG_USE_INFO_LED
   #define INFO_LED_GPIO      CONFIG_INFO_LED_GPIO
#else
   #undef INFO_LED_GPIO
#endif

// --------------------------------------------------------------------------
// variables
// --------------------------------------------------------------------------

led_config_t  led_config[NUM_LEDS];
led_state_t   led_state[NUM_LEDS];

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR leds_init( void )
{
   ESP_LOGD( TAG, "leds_init" );

   memset( led_config, 0, sizeof( led_config ) ); // all leds are off
   led_config[ SYS_LED ].mode = LED_NONE;    // red led
   led_config[ INFO_LED ].mode = LED_NONE;   // green led
   memset( led_state, 0, sizeof( led_state ) );

#ifdef SYS_LED_GPIO
   ESP_LOGD( TAG, "setup SYS_LED" );
   gpio_pad_select_gpio( SYS_LED_GPIO );
   /* Set the GPIO as a push/pull output */
   gpio_set_direction( SYS_LED_GPIO, GPIO_MODE_OUTPUT );
   // gpio_set_level( SYS_LED_GPIO, 0 );

   led_state[ SYS_LED ].pin = SYS_LED_GPIO;    // red led
#endif

#ifdef INFO_LED_GPIO
   ESP_LOGD( TAG, "setup INFO_LED" );
   gpio_pad_select_gpio( INFO_LED_GPIO );
   /* Set the GPIO as a input */
   gpio_set_direction( INFO_LED_GPIO, GPIO_MODE_INPUT );
   // gpio_set_level( INFO_LED_GPIO, 0 );

   led_state[ INFO_LED ].pin = INFO_LED_GPIO;  // green led
#endif
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void ICACHE_FLASH_ATTR led_set( uint8_t id, uint8_t mode, uint8_t on_time, uint8_t off_time, uint8_t repeat )
{
   ESP_LOGD( TAG, "led_set %d 0x%02x %d %d %d", id, mode, on_time, off_time, repeat );

   if( id >= NUM_LEDS )
      return;

   led_config[ id ].mode = mode;
   led_config[ id ].on_time = on_time;
   led_config[ id ].off_time = off_time;
   led_config[ id ].repeat = repeat;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

uint8_t ICACHE_FLASH_ATTR led_get( uint8_t id )
{
   return led_state[ id ].mode;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

bool ICACHE_FLASH_ATTR led_busy( uint8_t id )
{
   // ESP_LOGD( TAG, "led %d is busy %d || %d", id, led_config[ id ].mode != LED_NONE, led_state[ id ].flags.busy != 0 );

   return ( led_config[ id ].mode != LED_NONE || led_state[ id ].flags.busy != 0 ) ? true : false;
}

// --------------------------------------------------------------------------
// update leds
// --------------------------------------------------------------------------

// update leds in a 100 ms timer task

void ICACHE_FLASH_ATTR leds_update( void )
{
   // ESP_LOGD( TAG, "leds_update" );

   uint8_t id;

   for( id = 0; id < NUM_LEDS; id++ )
   {
      // Is there a new configuration and update is allowed?
      if( led_config[ id ].mode != LED_NONE && !led_state[ id ].flags.no_update )
      {
         ESP_LOGD( TAG, "led %d setup mode 0x%02x", id, led_config[ id ].mode );

         memcpy( &led_state[ id ], &led_config[id ], sizeof( led_config_t ) );
         led_config[ id ].mode = LED_NONE;      //  we have take over the configuration

         if( led_state[ id ].mode == LED_ON || led_state[ id ].mode == LED_OFF )
         {
            led_state[ id ].flags.led_on = led_state[ id ].mode == LED_ON ? 1 : 0;
            led_state[ id ].timer = 0;
            led_state[ id ].flags.flash_phase = 0;
            led_state[ id ].flags.no_update = 0;
            led_state[ id ].flags.busy = 0;
         }
         else
         {
            // setup timer for flashing or blinking
            led_state[ id ].flags.led_on = 1;                  // switch led on
            led_state[ id ].timer = led_state[ id ].on_time;   // time for on phase
            led_state[ id ].flags.flash_phase = 1;
            // allow update after one cycle of the inactive phase
            led_state[ id ].flags.no_update = 1;
            led_state[ id ].flags.busy = 1;
         }
      }
      else
      {
         // no new configuration or update not allowed
         if( led_state[ id ].mode == LED_BLINK || led_state[ id ].mode == LED_FLASH )
         {
            // do the flashing for the selected led
            led_state[ id ].timer--;

            if( led_state[ id ].flags.flash_phase == 0 )
               led_state[ id ].flags.no_update = 0;            // allow update again

            if( 0 == led_state[ id ].timer )
            {
               if( led_state[ id ].flags.flash_phase )
               {
                  // end of active phase
                  led_state[ id ].flags.led_on ^= 1;                 // toggle led -> off
                  led_state[ id ].flags.flash_phase = 0;
                  led_state[ id ].timer = led_state[ id ].off_time;  // time for off phase
                  if( led_state[ id ].timer == 0 )
                     led_state[ id ].timer = 1;
               }
               else
               {
                  // end of inactive phase
                  if( led_state[ id ].counter != 0 )
                  {
                     led_state[ id ].counter--;
                     if( led_state[ id ].counter == 0 )
                     {
                        led_state[ id ].mode = LED_OFF;        // this stops binking
                        led_state[ id ].flags.busy = 0;
                     }
                  }

                  if( led_state[ id ].off_time == 0 )
                  {
                     led_state[ id ].mode = LED_OFF;           // this stops flashing
                     led_state[ id ].flags.busy = 0;
                  }
                  else
                  {
                     led_state[ id ].flags.led_on ^= 1;                 // toggle led -> on
                     led_state[ id ].flags.flash_phase = 1;
                     led_state[ id ].timer = led_state[ id ].on_time;   // time for on phase
                  }
               }
            }
         }

         if( led_state[ id ].mode == LED_ON )
            led_state[ id ].flags.led_on = 1;
         else if( led_state[ id ].mode == LED_OFF )
            led_state[ id ].flags.led_on = 0;
      }

      // leds are connected to VCC, so invert pin value
      gpio_set_level( led_state[ id ].pin, led_state[ id ].flags.led_on ^ 1 );
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
