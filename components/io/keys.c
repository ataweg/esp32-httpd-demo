// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          jkey.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
// 2018-02-24  AWe   use an interrupt handler with a  message queue
//                   do the key processing in a extra task
// 2017-11-25  AWe   redesigned for proper working
//
// --------------------------------------------------------------------------

// see also Kolban's Book on ESP32 January 2018 - Page 303

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_WARN
static const char *TAG = "key";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <sys/time.h>
#include <rom/gpio.h>
#include "driver/gpio.h"
#include "keys.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static void IRAM_ATTR key_intr_handler( void *arg );

/******************************************************************************
 * FunctionName : single_key_init
 * Description  : init single key's gpio and register function
 * Parameters   : uint8_t gpio_num - which gpio to use
 *                key_function long_press - long press function, needed to install
 *                key_function short_press - short press function, needed to install
 * Returns      : single_key_param - single key parameter, needed by key init
*******************************************************************************/

struct single_key_param *single_key_init( uint8_t key_name, uint8_t gpio_num,
                                          key_function very_long_press, uint32_t press_very_long_time,
                                          key_function long_press,  uint32_t press_long_time,
                                          key_function short_press )
{
   struct single_key_param *single_key = ( struct single_key_param * )calloc( sizeof( struct single_key_param ), 1 );

   single_key->key_name             = key_name;
   single_key->gpio_num             = gpio_num;
   single_key->very_long_press      = very_long_press;
   single_key->press_very_long_time = press_very_long_time;
   single_key->long_press           = long_press;
   single_key->press_long_time      = press_long_time;
   single_key->short_press          = short_press;

   return single_key;
}

/******************************************************************************
 * FunctionName : key_init
 * Description  : init keys
 * Parameters   : key_param *keys - keys parameter, which inited by single_key_init
 * Returns      : none
*******************************************************************************/

esp_err_t keys_init( struct keys_param *keys, int key_num, struct single_key_param **single_key_list )
{
   esp_err_t err = ESP_OK;
   uint8_t i;

   keys->key_num = key_num;
   keys->single_key_list = single_key_list;

   for( i = 0; i < keys->key_num; i++ )
   {
      struct single_key_param *single_key = keys->single_key_list[i];

      ESP_LOGD( TAG, "Initializing GPIO pin %d", single_key->gpio_num );

      gpio_config_t gpioConfig;
      uint64_t pin_bit_mask = ( ( uint64_t ) 1 ) << single_key->gpio_num;
      gpioConfig.pin_bit_mask = pin_bit_mask;
      gpioConfig.mode         = GPIO_MODE_INPUT;
      gpioConfig.pull_up_en   = GPIO_PULLUP_ENABLE;
      gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
      gpioConfig.intr_type    = GPIO_INTR_NEGEDGE;
      gpio_config( &gpioConfig );

      // create a message queue for the ISR interaction
      keys->queue = xQueueCreate( 10, sizeof( struct single_key_param * ) );

      single_key->key_state = KEY_IDLE;
      single_key->key_counter = 0;
      single_key->key_queue = keys->queue;

      // enable interrupt on falling ( 1->0 ) edge for button pin
      ESP_LOGD( TAG, "Initializing GPIO interrupt for pin %d @core %d ", single_key->gpio_num, xPortGetCoreID() );
      gpio_set_intr_type( single_key->gpio_num, GPIO_INTR_NEGEDGE );
      gpio_intr_enable( single_key->gpio_num );

      // attach the interrupt service routine
      ESP_LOGD( TAG, "Attach Keys interrupt to GPIO ISR Handler" );
      err = gpio_isr_handler_add( single_key->gpio_num,
                                  key_intr_handler, ( void* ) single_key );

      if( err != ESP_OK )
      {
         ESP_LOGE( TAG, "gpio_isr_register for pin %d failed ( %x )", single_key->gpio_num, err );
         goto fail;
      }
   }
fail:
   return err;
}

/******************************************************************************
 * FunctionName : key_intr_handler
 * Description  : key interrupt handler
 * Parameters   : key_param *keys - keys parameter, which inited by single_key_init
 * Returns      : none
*******************************************************************************/

static void key_intr_handler( void *arg )
{
   struct single_key_param *single_key = ( struct single_key_param * ) arg;
   uint8_t gpio_num = single_key->gpio_num;
   uint8_t key_state = gpio_get_level( gpio_num ) ? KEY_UP : KEY_DOWN;

   // ESP_EARLY_LOGD( TAG, "key_intr_handler: Key %d, GPIO%1d, val: %d", single_key->key_name, gpio_num, key_state );
   // ESP_EARLY_LOGD( TAG, "GPIO.status 0x%08x core %d", GPIO.status, xPortGetCoreID() );
   // ESP_EARLY_LOGD( TAG, "GPIO.pin[%1d] 0x%08x core %d", single_key->gpio_num, GPIO.pin[ single_key->gpio_num ].val, xPortGetCoreID() );

   // disable interrupt
   gpio_intr_disable( gpio_num );

   switch( single_key->key_state )
   {
      case KEY_UP:               // key was released
      case KEY_RELEASED:         // key is released
         if( key_state == KEY_DOWN )
            single_key->key_state = KEY_PRESSED;
         else
            single_key->key_state = KEY_UP;
         break;

      case KEY_PRESSED:          // key is pressed
      case KEY_DOWN:             // key was pressed
         if( key_state == KEY_UP )
            single_key->key_state = KEY_RELEASED;
         else
            single_key->key_state = KEY_DOWN;
         break;
   }

   BaseType_t higher_priority_task_woken = false;
   BaseType_t ret = xQueueSendToBackFromISR( single_key->key_queue, &single_key, &higher_priority_task_woken );
   if( ret != pdTRUE )
   {
      ESP_EARLY_LOGW( TAG, "queue send failed ( %d ), key %d", ret,  single_key->key_name );
   }

   // gpio_set_intr_type( gpio_num, GPIO_PIN_INTR_ANYEDGE );
   // ESP_EARLY_LOGD( TAG, "Initializing GPIO interrupt for pin %d @core %d ", gpio_num, xPortGetCoreID() );
   // gpio_intr_enable( gpio_num );
   // enable interrupt in key_process()

   if( ret == pdTRUE && higher_priority_task_woken == pdTRUE )
   {
      portYIELD_FROM_ISR();
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// like key_50ms_cb()
// !!! todo ( AWe ): when we have more than one key, also check the state of the other keys

void keys_process( struct keys_param *keys )
{
   struct single_key_param *single_key;
   xQueueReceive( keys->queue, &single_key, portMAX_DELAY );

   ESP_LOGD( TAG, "Key %d %1d=%s", single_key->key_name, single_key->key_state,
             single_key->key_state == KEY_PRESSED  ? "pressed"  :
             single_key->key_state == KEY_DOWN     ? "down"     :
             single_key->key_state == KEY_RELEASED ? "released" :
                                                     "up" );

   struct timeval now;
   gettimeofday( &now, NULL );
   uint32_t now_ms =  now.tv_sec * 1000 + now.tv_usec / 1000;;

   if( single_key->key_state >= KEY_RELEASED )
   {
      // key is up
      single_key->key_state = KEY_IDLE;

      uint32_t duration = now_ms - single_key->key_counter;

      if( single_key->very_long_press && duration > single_key->press_very_long_time )        // 10 sec
      {
         ESP_LOGD( TAG, "Registered a very long press %d", duration );
         single_key->very_long_press();
      }
      else if( single_key->long_press && duration > single_key->press_long_time )        // 2 sec
      {
         ESP_LOGD( TAG, "Registered a long press %d", duration );
         single_key->long_press();
      }
      else if( single_key->short_press )
      {
         ESP_LOGD( TAG, "Registered a short press %d", duration );
         single_key->short_press();
      }
      single_key->key_counter = now_ms;
      gpio_set_intr_type( single_key->gpio_num, GPIO_INTR_NEGEDGE );
   }
   else if( single_key->key_state == KEY_PRESSED )
   {
      // key is down
      single_key->key_state = KEY_DOWN;
      single_key->key_counter = now_ms;
      gpio_set_intr_type( single_key->gpio_num, GPIO_INTR_POSEDGE );
   }
   // ESP_LOGD( TAG, "Enable interrupt %d @core %d", single_key->gpio_num, xPortGetCoreID() );
   gpio_intr_enable( single_key->gpio_num );
   // ESP_LOGD( TAG, "GPIO.status 0x%08x core %d", GPIO.status, xPortGetCoreID() );
   // ESP_LOGD( TAG, "GPIO.pin[%1d] 0x%08x core %d", single_key->gpio_num, GPIO.pin[ single_key->gpio_num ].val, xPortGetCoreID() );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

void keys_deinit( struct keys_param *keys )
{
   if( keys->queue )
   {
      vQueueDelete( keys->queue );
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
