// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things - Webserver
//
// File          app_main.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
// 2018-03-20  AWe   move wifi stuff to app_wifi.c
// 2018-02-24  AWe   change "appHttpServer" task prority from 5 to 4
//                   change "appHttpServer" task size from 4096 to 2048
// 2018-02-22  AWe   add tasks for leds and buttons
// 2017-09-20  AWe   functions in esp_heap_alloc_caps.h are deprecated; use functions from esp_heap_caps.h
//
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_DEBUG
static const char* TAG = "WebServer";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <string.h>
#include <stdio.h>
#include <sys/time.h>               // struct timeval
#include <byteswap.h>
#include <rom/rtc.h>                // rtc_get_wakeup_cause()

#include "freertos/FreeRTOS.h"      // xPortGetFreeHeapSize()
#include "freertos/event_groups.h"

#include "esp_heap_caps.h"          // heap_caps_get_largest_free_block(), heap_caps_get_minimum_free_size()
#include "esp_clk.h"                // esp_clk_cpu_freq()
#include "nvs_flash.h"              // nvs_flash_init()
#include "esp_spi_flash.h"          // spi_flash_get_chip_size()
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "lwip/api.h"
#include "driver/spi_master.h"

#include "leds.h"
#include "app_config.h"

// --------------------------------------------------------------------------
// heap tracing
/*
 * esp_err_t heap_trace_init_standalone( heap_trace_record_t *record_buffer, size_t num_records );
 * esp_err_t heap_trace_start( heap_trace_mode_t mode );
 * esp_err_t heap_trace_stop( void );
 - esp_err_t heap_trace_resume( void );
 - size_t heap_trace_get_count( void );
 - esp_err_t heap_trace_get( size_t index, heap_trace_record_t *record );
 * void heap_trace_dump( void );
*/

#ifdef CONFIG_HEAP_TRACING
   #define TRACE_ENABLE

   #ifdef TRACE_ENABLE
      #include "esp_heap_trace.h"

      #define NUM_RECORDS 64
      static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM
   #endif
#endif

// --------------------------------------------------------------------------
// app_http_server

void appHttpServer( void *pvParameters );

// --------------------------------------------------------------------------
// leds & buttons  ( io.c )

void leds_init( void );

// --------------------------------------------------------------------------
//  app_button.h

void appButtonTask( void *pvParameter );
void appLedTask( void *pvParameter );

// --------------------------------------------------------------------------
// app_wifi.c

void wifiIinitialize( void );

extern ip4_addr_t s_ip_addr;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// ESP32 Memory Map
// +-----------------+-------------+-------------+--------+--------------------+
// | Target          |Start Address| End Address | Size   |Target              |
// +-----------------+-------------+-------------+--------+--------------------+
// | Internal SRAM 0 | 0x4007_0000 | 0x4009_FFFF | 192 kB | Instruction, Cache |
// +-----------------+-------------+-------------+--------+--------------------+
// | Internal SRAM 1 | 0x3FFE_0000 | 0x3FFF_FFFF | 128 kB | Data, DAM          |
// |                 | 0x400A_0000 | 0x400B_FFFF |        | Instruction        |
// +-----------------+-------------+-------------+--------+--------------------+
// | Internal SRAM 2 | 0x3FFA_E000 | 0x3FFD_FFFF | 200 kB | Data, DMA          |
// +-----------------+-------------+-------------+--------+---------------------+

// see also C:\Espressif\esp-idf\components\soc\esp32\soc_memory_layout.c

void printHeapInfo( void )
{
   size_t free8min, free32min, free8, free32;

   // heap_caps_print_heap_info( MALLOC_CAP_8BIT );
   // heap_caps_print_heap_info( MALLOC_CAP_32BIT );

   free8  = heap_caps_get_largest_free_block( MALLOC_CAP_8BIT );
   free32 = heap_caps_get_largest_free_block( MALLOC_CAP_32BIT );
   free8min  = heap_caps_get_minimum_free_size( MALLOC_CAP_8BIT );
   free32min = heap_caps_get_minimum_free_size( MALLOC_CAP_32BIT );

   ESP_LOGD( TAG, "Free heap: %u bytes", xPortGetFreeHeapSize() );
   ESP_LOGD( TAG, "Free ( largest free blocks ) 8bit-capable memory : %6d bytes, 32-bit capable memory %6d bytes", free8, free32 );
   ESP_LOGD( TAG, "Free ( min free size ) 8bit-capable memory       : %6d bytes, 32-bit capable memory %6d bytes", free8min, free32min );

   ESP_LOGD( TAG, "task stack: %d bytes", uxTaskGetStackHighWaterMark( NULL ) );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

RESET_REASON reset_reason[2];
WAKEUP_REASON wakeup_reason;

static void printSystemInfo()
{
   esp_chip_info_t chip_info;
   struct timeval tv;

   /* Print chip information */
   esp_chip_info( &chip_info );

   gettimeofday( &tv, NULL );

   printf( "\r\n\r\n" );
   printf( "-------------------------------------------\r\n" );
   printf( "BOOTUP...\r\n" );
   printf( "\r\n" );
   printf( "ESP32 platform starting...\r\n" );
   printf( "==== System info: ====\r\n" );
   printf( "IDF version:   %s\r\n", esp_get_idf_version() );
   printf( "Project:       ESP32/Firmware/WebServer\r\n" );
   printf( "Build time:    " __DATE__ " " __TIME__ "\r\n" );
   printf( "Time           = %ld\r\n", ( long int )tv.tv_sec );
   printf( "silicon rev.   = %d\r\n", chip_info.revision );
   printf( "Num cores      = %d\r\n",  chip_info.cores );
   printf( "Features       = WiFi%s%s\r\n", ( chip_info.features & CHIP_FEATURE_BT ) ? "/BT"   : "",
                                            ( chip_info.features & CHIP_FEATURE_BLE ) ? "/BLE" : "" );
   printf( "CPU freq       = %d MHz\r\n", esp_clk_cpu_freq() / 1000 / 1000 );
   printf( "Flash size     = %dMB %s\r\n", spi_flash_get_chip_size() / ( 1024 * 1024 ),
                                           ( chip_info.features & CHIP_FEATURE_EMB_FLASH ) ? "embedded" : "external" );
   printf( "Free heap size = %d bytes\r\n", esp_get_minimum_free_heap_size() );
   printf( " ==== End System info ====\r\n" );
   printf( " -------------------------------------------\r\n\r\n" );
}

void app_main()
{
   // esp_log_level_set( "*", ESP_LOG_VERBOSE );
   // esp_log_level_set( TAG, ESP_LOG_DEBUG );

   esp_err_t err = ESP_OK;

   reset_reason[0] = rtc_get_reset_reason( 0 );
   reset_reason[1] = rtc_get_reset_reason( 1 );
   wakeup_reason   = rtc_get_wakeup_cause();

   // ----------------------------------
   // print system info
   // ----------------------------------

   printSystemInfo();

   // const char *version = esp_get_idf_version();

   // ----------------------------------
   // init nvs flash
   // ----------------------------------

   ESP_LOGD( TAG, "Starting nvs_flash_init" );
   nvs_flash_init();

   // ----------------------------------
   // leds init
   // ----------------------------------

   ESP_LOGI( TAG, "Setup leds and buttons ... " );
   leds_init();
   SysLed_On();
   InfoLed_On();

   // ----------------------------------
   // setup gpio interrupt handler
   // ----------------------------------
   // gpio interru8pt sources
   // * WPS_BTN ( key.c )

   // install gpio isr service
   ESP_LOGI( TAG, "Install gpio isr service ... " );
   err = gpio_install_isr_service( /*ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_LEVEL1 |*/ ESP_INTR_FLAG_IRAM );
   if( ESP_OK != err )
   {
      ESP_LOGE( TAG, "gpio_install_isr_service failed ( %x )", err );
   }

   // ----------------------------------
   // start led and button task
   // ----------------------------------

   xTaskCreate( &appLedTask, "appLedTask",  1024, NULL, 4, NULL );
   // due to a bug in the IDF we have to pin the task to the same cpu where the ISR is running
   xTaskCreatePinnedToCore( &appButtonTask, "appButtonTask", 2560, NULL, 5, NULL, 0 ); // must pinned to core 0

   // ----------------------------------
   // setup wifi
   // ----------------------------------
   ESP_LOGD( TAG, "Setup Wifi ..." );

   wifiIinitialize();

   // VERY UNSTABLE without this delay after init'ing wifi...
   // however, much more stable with a new Power Supply
   // vTaskDelay( 1000 / portTICK_RATE_MS );

   ESP_LOGD( TAG, "Wifi Initialized..." );
   // ESP_LOGD( TAG, "Free heap: %u", xPortGetFreeHeapSize() );

   Led_Flash_Slow( SYS_LED );

   // ----------------------------------
   // start http server task
   // ----------------------------------

#ifdef TRACE_ENABLE
   ESP_LOGI( TAG, "Starting tracing appHttpServer task..." );
   ESP_ERROR_CHECK( heap_trace_init_standalone( trace_record, NUM_RECORDS ) );
#endif

   // ESP_LOGD( TAG, "Starting appHttpServer task..." );

   // keep an eye on stack... 5784 min with 8048 stack size last count..
   xTaskCreatePinnedToCore( &appHttpServer, "appHttpServer", 2048, NULL, 6, NULL, 1 );

   // ----------------------------------
   // system and application setup done
   // ----------------------------------

   // ESP_LOGD( TAG, "Free heap: %u", xPortGetFreeHeapSize() );
   ESP_LOGI( TAG, "ready ..." );

   // printHeapInfo();
   ESP_LOGI( TAG, "--------------------------------------------------------------------------\r\n" );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
