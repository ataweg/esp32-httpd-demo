#ifndef __KEY_H__
#define __KEY_H__

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "driver/gpio.h"      // intr_handle_t

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

typedef void ( * key_function )( void );

//  _____           _____
//       |_________|
//  0 -> 1 -> 2 -> 3 -> 0

#define KEY_UP          0
#define KEY_PRESSED     1
#define KEY_DOWN        3
#define KEY_RELEASED    2
#define KEY_IDLE        KEY_UP

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

struct single_key_param
{
   uint8_t  key_name;         // index of key
   uint8_t  key_state;        //
   uint8_t  gpio_num;         // which gpio pin to use
   uint8_t  dmy[1];           //
   uint32_t key_counter;      //

   key_function short_press;  // short press function, needed to install
   key_function long_press;   // long press function, needed to install
   uint32_t press_long_time;
   key_function very_long_press;
   uint32_t press_very_long_time;
   intr_handle_t intr_handle;
   QueueHandle_t key_queue;
};

struct keys_param
{
   uint8_t key_num;
   struct single_key_param **single_key_list; // pointer to an array
   QueueHandle_t queue;
};

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

struct single_key_param *single_key_init( uint8_t key_name, uint8_t gpio_num,
                                          key_function very_long_press, uint32_t press_very_long_time,
                                          key_function long_press,  uint32_t press_long_time,
                                          key_function short_press );
esp_err_t keys_init( struct keys_param *keys, int key_num, struct single_key_param **single_key_list );
void keys_deinit( struct keys_param *keys );
void keys_process( struct keys_param *keys );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#endif  // __KEY_H__
