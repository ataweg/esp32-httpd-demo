// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things - Wifi Camera -Webserver
//
// File          app_httpd.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
// 2017-12-17  AWe   remove support for 2nd websocket
// 2017-09-20  AWe   functions in esp_heap_alloc_caps.h are deprecated; use functions from esp_heap_caps.h
// 2017-10-02  AWe   take souces from
//                     https://github.com/chmorgan/libesphttpd_linux_example/blob/master/httpd/main.c
//
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_DEBUG
static const char* TAG = "app_httpd";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "libesphttpd/esp.h"
#include "libesphttpd/httpd.h"
#include "libesphttpd/httpdespfs.h"
#include "libesphttpd/cgiwifi.h"
#include "libesphttpd/cgiflash.h"
#include "libesphttpd/espfs.h"
#include "libesphttpd/webpages-espfs.h"
#include "libesphttpd/cgiwebsocket.h"
#include "libesphttpd/auth.h"
#include "libesphttpd/captdns.h"

#include <libesphttpd/linux.h>
#include <libesphttpd/httpd-freertos.h>
#include <libesphttpd/cgiredirect.h>
#include <libesphttpd/route.h>

#include "leds.h"
#include "cgi.h"
#include "cgi-test.h"

// --------------------------------------------------------------------------
// heap tracing

//#define TRACE_ON

#ifdef CONFIG_HEAP_TRACING
  #ifdef TRACE_ON
     #include "esp_heap_trace.h"
  #endif
#endif

// --------------------------------------------------------------------------
//  Prototypes
// --------------------------------------------------------------------------

static int  ICACHE_FLASH_ATTR appPassFn( HttpdConnData *connData, int no, char *user, int userLen, char *pass, int passLen );
static void ICACHE_FLASH_ATTR appWebsocketRecv( Websock *ws, char *data, int len, int flags );
static void ICACHE_FLASH_ATTR appWebsocketConnect( Websock *ws );
static void ICACHE_FLASH_ATTR appEchoWebsocketRecv( Websock *ws, char *data, int len, int flags );
static void ICACHE_FLASH_ATTR appEchoWebsocketConnect( Websock *ws );
static void* ICACHE_FLASH_ATTR appWebsocketBcast( void *arg );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static HttpdFreertosInstance httpdFreertosInstance;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

/*
This is the main url->function dispatching data struct.
In short, it's a struct with various URLs plus their handlers. The handlers can
be 'standard' CGI functions you wrote, or 'special' CGIs requiring an argument.
They can also be auth-functions. An asterisk will match any url starting with
everything before the asterisks; "*" matches everything. The list will be
handled top-down, so make sure to put more specific rules above the more
general ones. Authorization things ( like authBasic ) act as a 'barrier' and
should be placed above the URLs they protect.
*/

const HttpdBuiltInUrl builtInUrls[] ICACHE_RODATA_ATTR STORE_ATTR =
{
// url                     | SendCallback                   | Arguments
// const char *url;        | cgiSendCallback cgiCb;         | const void *cgiArg, cgiArg2;
// ------------------------|--------------------------------|----------------------
// {"*",                      cgiRedirectApClientToHostname,   "esp8266.nonet", NULL },
   {"/",                      cgiRedirect,                     "/index.tpl", NULL },
   {"/led.tpl",               cgiEspFsTemplate,                tplLed, NULL },
   {"/index.tpl",             cgiEspFsTemplate,                tplCounter, NULL },
   {"/led.cgi",               cgiLed,                          NULL, NULL },

   // Routines to make the /wifi URL and everything beneath it work.
   // Enable the line below to protect the WiFi configuration with an username/password combo.
// {"/wifi/*",                authBasic,                       appPassFn, NULL },

   {"/wifi",                  cgiRedirect,                     "/wifi/wifi.tpl", NULL },
   {"/wifi/",                 cgiRedirect,                     "/wifi/wifi.tpl", NULL },
   {"/wifi/wifiscan.cgi",     cgiWiFiScan,                     NULL, NULL },     // called from WifiSetup.tpl.html
   {"/wifi/wifi.tpl",         cgiEspFsTemplate,                tplWlan, NULL },
   {"/wifi/connect.cgi",      cgiWiFiConnect,                  NULL, NULL },     // called from WifiSetup.tpl.html
   {"/wifi/connstatus.cgi",   cgiWiFiConnStatus,               NULL, NULL },     // called from wifi/connecting.html
   {"/wifi/setmode.cgi",      cgiWiFiSetMode,                  NULL, NULL },     // not used

   {"/websocket/ws.cgi",      cgiWebsocket,                    appWebsocketConnect, NULL },
   {"/websocket/echo.cgi",    cgiWebsocket,                    appEchoWebsocketConnect, NULL },  // not used

   {"/test",                  cgiRedirect,                     "/test/index.html", NULL },
   {"/test/",                 cgiRedirect,                     "/test/index.html", NULL },
   {"/test/test.cgi",         cgiTestbed,                      NULL, NULL },

#ifdef INCLUDE_FLASH_FNS
   {"/flash/next",            cgiGetFirmwareNext,              &uploadParams, NULL },
   {"/flash/upload",          cgiUploadFirmware,               &uploadParams, NULL },
#endif
   {"/flash/reboot",          cgiRebootFirmware,               NULL, NULL },

   {"*",                      cgiEspFsHook,                    NULL, NULL },     // Catch-all cgi function for the filesystem
   {NULL, NULL, NULL, NULL}
};

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// Function that tells the authentication system what users/passwords live on the system.
// This is disabled in the default build; if you want to try it, enable the authBasic line in
// the builtInUrls below.

static int ICACHE_FLASH_ATTR appPassFn( HttpdConnData *connData, int no, char *user, int userLen, char *pass, int passLen )
{
   if( no == 0 )
   {
      strcpy( user, "admin" );
      strcpy( pass, "welc0me" );
      return 1;
// Add more users this way. Check against incrementing no for each user added.
// } else if ( no==1 ) {
//    os_strcpy( user, "user1" );
//    os_strcpy( pass, "something" );
//    return 1;
   }
   return 0;
}

// --------------------------------------------------------------------------
// Websocket
// --------------------------------------------------------------------------

//Broadcast the uptime in seconds every second over connected websockets

static void* ICACHE_FLASH_ATTR appWebsocketBcast( void *arg )
{
   char buf[256];
   static int count = 0;

   while( 1 )
   {
      count++;
      int buflen = snprintf( buf, sizeof( buf ), "Up for %d minutes %d seconds!\n", count / 60, count % 60 );

      if( buflen < sizeof( buf ) )
         cgiWebsockBroadcast( &httpdFreertosInstance.httpdInstance, "/websocket/ws.cgi", buf, buflen, WEBSOCK_FLAG_NONE );

      sleep( 1 );
   }

   return NULL;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// On reception of a message, send "You sent: " plus whatever the other side sent

static void ICACHE_FLASH_ATTR appWebsocketRecv( Websock *ws, char *data, int len, int flags )
{
   char buf[256];

   int buflen = snprintf( buf, sizeof( buf ), "You sent: " );

   for( int i = 0; i < len; i++ )
      buf[ buflen++ ] = data[ i ];
   buf[ buflen++ ] = 0;

   cgiWebsocketSend( &httpdFreertosInstance.httpdInstance, ws, buf, buflen, WEBSOCK_FLAG_NONE );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// Websocket connected. Install reception handler and send welcome message.

static void ICACHE_FLASH_ATTR appWebsocketConnect( Websock *ws )
{
   ESP_LOGD( TAG, "Websocket: connect" );

   ws->recvCb = ( WsRecvCb ) appWebsocketRecv;
   cgiWebsocketSend( &httpdFreertosInstance.httpdInstance, ws, "Hi, Websocket!", 14, WEBSOCK_FLAG_NONE );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// On reception of a message, echo it back verbatim

static void ICACHE_FLASH_ATTR appEchoWebsocketRecv( Websock *ws, char *data, int len, int flags )
{
   ESP_LOGD( TAG, "EchoWs: echo, len=%d", len );
   cgiWebsocketSend( &httpdFreertosInstance.httpdInstance, ws, data, len, flags );
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// Echo websocket connected. Install reception handler.

static void ICACHE_FLASH_ATTR appEchoWebsocketConnect( Websock *ws )
{
   ESP_LOGD( TAG, "EchoWs: connect" );
   ws->recvCb = ( WsRecvCb ) appEchoWebsocketRecv;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int appHttpServer( void )
{
#if 0 // don't use captdnsInit()
   // don't use it
   // DNS server: it basically replies with a fixed IP, the one of the SoftAP
   // interface of this ESP module ) for any and all DNS queries.
   captdnsInit();
#endif

   // 0x40200000 is the base address for spi flash memory mapping, ESPFS_POS is the position
   // where image is written in flash that is defined in Makefile.
#ifdef ESPFS_POS
   espFsInit( ( void* )( 0x40200000 + ESPFS_POS ) );
#else
   espFsInit( ( void* )( webpages_espfs_start ) );
#endif

   // allocate memory for the commenction; 3180 byte per connection
   const int maxConnections = HTTPD_MAX_CONNECTIONS;

   int connectionMemorySize = sizeof( RtosConnType ) * maxConnections;
   char *connectionMemory = malloc( connectionMemorySize );
   ESP_LOGD( TAG, "Allocate %d bytes of memory for %1d connections at 0x%08x", connectionMemorySize, maxConnections, ( uint32_t )connectionMemory );

   if( NULL == connectionMemory )
   {
      ESP_LOGE( TAG, "Cannot allocate %d bytes for connection memory", connectionMemorySize );

      return 0;
   }

   // start the http daemon
#ifndef CONFIG_ESPHTTPD_SSL_SUPPORT
   int listenPort = 80;
   httpdFreertosInit( &httpdFreertosInstance, builtInUrls, listenPort,
                      connectionMemory, maxConnections, HTTPD_FLAG_NONE );
#else
   int listenPort = 443;
   httpdFreertosInit( &httpdFreertosInstance, builtInUrls, listenPort,
                      connectionMemory, maxConnections, HTTPD_FLAG_SSL );
#endif
   ESP_LOGD( TAG, "creating httpd on port %d", listenPort );

   ESP_LOGD( TAG, "creating websocket broadcast thread" );
   pthread_t websocketThread;  // thread attribute are not supported
   pthread_create( &websocketThread, NULL, appWebsocketBcast, NULL );

   while( true )
   {
      // ESP_LOGD( TAG, "Running\r\n" );
      sleep( 10 );  // in seconds
   }

   free( connectionMemory );
   return 0;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#if 0 // no traces

#ifdef TRACE_ON
      ESP_LOGI( TAG, "Start heap tracing..." );
      ESP_ERROR_CHECK( heap_trace_start( HEAP_TRACE_ALL ) ); // HEAP_TRACE_ALL, HEAP_TRACE_LEAKS,
#endif


#ifdef TRACE_ON
      ESP_LOGI( TAG, "Stop heap tracing" );
      ESP_ERROR_CHECK( heap_trace_stop() );
      ESP_LOGI( TAG, "Dump heap trace" );
      heap_trace_dump();
#endif
#endif