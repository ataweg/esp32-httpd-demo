// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File          user_http.c
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-01-19  AWe   replace httpd_printf() with ESP_LOG*()
//    2017-08-24  AWe   replace
//                      resetTimer      --> staCheckTimer
//                      resetTimerCb    --> staWifiCheckConnStatusCb
//                      reassTimerCb    --> staWiFiDoConnectCb
//
//                      reformat some parts
//                      shrink some buffers for sprintf()
//    2017-08-23  AWe   take over changes from MightyPork/libesphttpd
//                        https://github.com/MightyPork/libesphttpd/commit/3237c6f8fb9fd91b22980116b89768e1ca21cf66
//
// --------------------------------------------------------------------------

#ifdef ESP_PLATFORM

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Cgi/template routines for the /wifi url.
*/

// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------

#define LOG_LOCAL_LEVEL    ESP_LOG_DEBUG
static const char *TAG = "esp32_cgiWifi";
#include "esp_log.h"
#define S( str ) ( str == NULL ? "<null>": str )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"

#include "libesphttpd/esp.h"
#include "libesphttpd/cgiwifi.h"
#include "httpd-platform.h"
#include "app_wifi.h"

// --------------------------------------------------------------------------
// prototypes
// --------------------------------------------------------------------------

// helper functions
int ICACHE_FLASH_ATTR rssi2perc( int rssi );
const ICACHE_FLASH_ATTR char *auth2str( int auth );
const ICACHE_FLASH_ATTR char *opmode2str( int opmode );

static void ICACHE_FLASH_ATTR staWifiCheckConnStatusCb( void *arg );
static void ICACHE_FLASH_ATTR staWiFiDoConnectCb( void *arg );

// called from website
CgiStatus ICACHE_FLASH_ATTR cgiWiFiScan( HttpdConnData *connData );
CgiStatus ICACHE_FLASH_ATTR cgiWiFiConnect( HttpdConnData *connData );
CgiStatus ICACHE_FLASH_ATTR cgiWiFiConnStatus( HttpdConnData *connData );
CgiStatus ICACHE_FLASH_ATTR cgiWiFiSetMode( HttpdConnData *connData );
CgiStatus ICACHE_FLASH_ATTR cgiWiFiSetChannel( HttpdConnData *connData );

CgiStatus ICACHE_FLASH_ATTR tplWlan( HttpdConnData *connData, char *token, void **arg );

// --------------------------------------------------------------------------
// defines
// --------------------------------------------------------------------------

// Enable this to disallow any changes in AP settings
// #define DEMO_MODE

#define GOOD_IP2STR( ip ) ( (ip )>>0 )&0xff, ( (ip )>>8 )&0xff,  ( (ip )>>16 )&0xff, ( (ip )>>24 )&0xff

#define CONNTRY_IDLE       0
#define CONNTRY_WORKING    1
#define CONNTRY_SUCCESS    2
#define CONNTRY_FAIL       3

#define SCAN_IDLE          0
#define SCAN_RUNING        1
#define SCAN_DONE          2
#define SCAN_INVALID       3

// --------------------------------------------------------------------------
// external variables
// --------------------------------------------------------------------------

extern EventGroupHandle_t wifi_event_group;
extern const int WIFI_SCAN_DONE;
extern const int WIFI_STA_CONNECTED;
extern const int WIFI_STA_DISCONNECT;

// --------------------------------------------------------------------------
// local typedefs and variables
// --------------------------------------------------------------------------

// Scan result
typedef struct
{
   char scanInProgress;       // if 1, don't access the underlying stuff from the webpage.
   wifi_ap_record_t *apData;
   int noAps;
} ScanResultData;

// Static scan status storage.
static ScanResultData cgiWifiAps  = { 0 };

// Connection result var
static int connTryStatus = CONNTRY_IDLE;

// Temp store for new ap info.
static wifi_config_t wifi_config_cgi = { 0 };

static HttpdPlatTimerHandle staCheckTimer;
static HttpdPlatTimerHandle staConnectTimer;

// --------------------------------------------------------------------------
// taken from MightyPork/libesphttpd
// --------------------------------------------------------------------------

int ICACHE_FLASH_ATTR rssi2perc( int rssi )
{
   int r;

   if( rssi > 200 )
      r = 100;
   else if( rssi < 100 )
      r = 0;
   else
      r = 100 - 2 * ( 200 - rssi ); // approx.

   if( r > 100 ) r = 100;
   if( r < 0 ) r = 0;

   return r;
}

const ICACHE_FLASH_ATTR char *auth2str( int auth )
{
   switch( auth )
   {
      case WIFI_AUTH_OPEN:          return "Open";
      case WIFI_AUTH_WEP:           return "WEP";
      case WIFI_AUTH_WPA_PSK:       return "WPA";
      case WIFI_AUTH_WPA2_PSK:      return "WPA2";
      case WIFI_AUTH_WPA_WPA2_PSK:  return "WPA/WPA2";
      default:                      return "Unknown";
   }
}

const ICACHE_FLASH_ATTR char *opmode2str( int opmode )
{
   switch( opmode )
   {
      case WIFI_MODE_NULL:    return "Disabled";
      case WIFI_MODE_STA:     return "Client";
      case WIFI_MODE_AP:      return "AP only";
      case WIFI_MODE_APSTA:   return "Client+AP";
      default:                return "Unknown";
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// This routine is ran some time after a connection attempt to an access point. If
// the connect succeeds, this gets the module in STA-only mode.

static void ICACHE_FLASH_ATTR staWifiCheckConnStatusCb( void *arg )
{
   ESP_LOGD( TAG, "staWifiCheckConnStatusCb" );
   int bits = xEventGroupWaitBits( wifi_event_group, WIFI_STA_CONNECTED, 0, 0, 0 );
   if( bits & WIFI_STA_CONNECTED )
   {
      tcpip_adapter_ip_info_t ipInfo;
      tcpip_adapter_get_ip_info( TCPIP_ADAPTER_IF_STA, &ipInfo );

      ESP_LOGD( TAG, "Got IP "IPSTR". Going into STA mode ...", GOOD_IP2STR( ipInfo.ip.addr ) );
      ESP_LOGI( TAG, "Connecting to \"%s\"", wifi_config_cgi.sta.ssid );

      // Go to STA mode. Leave AP mode
      // !!! todo ( AWe ): when coming from only STA mode, we have no change to get the new IP address
      //                    so we need here something tricky

      wifiSetNewMode( WIFI_MODE_STA );
      wifiSetConfig( WIFI_IF_STA, &wifi_config_cgi );
   }
   else
   {
      connTryStatus = CONNTRY_FAIL;
      ESP_LOGE( TAG, "Connect fail. Not going into STA-only mode." );
      // !!! todo ( AWe ): Maybe also pass this through on the webpage?
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// Actually connect to a station. This routine is timed because I had problems
// with immediate connections earlier. It probably was something else that caused it,
// but I can't be arsed to put the code back :P

static void ICACHE_FLASH_ATTR staWiFiDoConnectCb( void *arg )
{
   ESP_LOGI( TAG, "staWiFiDoConnectCb: to AP \"%s\" pw \"%s\"", wifi_config_cgi.sta.ssid, wifi_config_cgi.sta.password );

   ESP_LOGD( TAG, "Wifi disconnect ..." );
   wifiDisconnect();  // waits for disconnected

   ESP_LOGD( TAG, "Wifi set config" );
   // AP mode when set
   wifi_mode_t mode;
   wifiGetCurrentMode( &mode );

   // keep AP mode, when set
   if( mode & WIFI_MODE_AP )
   {
      ESP_LOGD( TAG, "Wifi set mode to APSTA" );
      mode = WIFI_MODE_APSTA;
   }
   else
   {
      ESP_LOGD( TAG, "Wifi set mode to STA" );
      mode = WIFI_MODE_STA;
   }
   wifiSetNewMode( mode );
   wifiSetConfig( WIFI_IF_STA, &wifi_config_cgi );

   ESP_LOGD( TAG, "Connect...." );
   if( wifiConnect() )
   {
      connTryStatus = CONNTRY_WORKING;
      wifiGetCurrentMode( &mode );
      if( mode != WIFI_MODE_STA )
      {
         // Schedule disconnect/connect
         // time out after 60 secs of trying to connect
         ESP_LOGD( TAG, "staCheckTimer ( 60000 )...." );

         staCheckTimer = httpdPlatTimerCreate( "staCheckTimer", 60000, 0, staWifiCheckConnStatusCb, NULL );
         httpdPlatTimerStart( staCheckTimer );
      }
   }
   else
   {
      ESP_LOGE( TAG, "Connect Error" );
   }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// This CGI is called from the bit of AJAX-code in wifi.tpl. It will initiate a
// scan for access points and if available will return the result of an earlier scan.
// The result is embedded in a bit of JSON parsed by the javascript in wifi.tpl.

CgiStatus ICACHE_FLASH_ATTR cgiWiFiScan( HttpdConnData *connData )
{
   int pos = ( int )connData->cgiData;
   int len;
   char buff[256];

   ESP_LOGD( TAG, "cgiWiFiScan%s state %d pos %d ...", cgiWifiAps.scanInProgress ? " in propgress ..." : "", cgiWifiAps.scanInProgress, pos );

   if( cgiWifiAps.scanInProgress == SCAN_IDLE )
   {         // idle state -> start scaning
      wifi_mode_t mode;
      wifiGetCurrentMode( &mode );
      if( mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA )
      {            // Start a new scan.
         ESP_LOGD( TAG, "Start a new scan ..." );
         wifiStartScan();
         connData->cgiData = ( void* )0;  // set pos to zero
         cgiWifiAps.scanInProgress = SCAN_RUNING;
      }
      else
      {
         ESP_LOGE( TAG, "Cannot start a new scan because not in a station mode" );
         return HTTPD_CGI_DONE;
      }
   }

   if( cgiWifiAps.scanInProgress == SCAN_RUNING )
   {      // scan is runing, wait for done
      httpdStartResponse( connData, 200 );
      httpdHeader( connData, "Content-Type", "application/json" );
      httpdEndHeaders( connData );

      ESP_LOGD( TAG, "wifi scan ready?" );
      if( xEventGroupGetBits( wifi_event_group ) & WIFI_SCAN_DONE )
      {         // wifi scan done
         ESP_LOGD( TAG, "wifi scan done" );

         // Clear prev ap data if needed.
         if( cgiWifiAps.apData != NULL )
            free( cgiWifiAps.apData );

         cgiWifiAps.noAps = wifiScanDone( &cgiWifiAps.apData ); // allocates memory
         if( cgiWifiAps.apData == NULL )
         {
            ESP_LOGD( TAG, "Cannot allocate memory for scan result" );
            cgiWifiAps.noAps = 0;
         }

         cgiWifiAps.scanInProgress = SCAN_DONE;
         connData->cgiData = ( void * )1;  // => pos: send the scan result as json body

         // update web page
         ESP_LOGD( TAG, "We have a scan result" );
         // We have a scan result. Send json header
         len = sprintf( buff, "{\n \"result\": { \n\"inProgress\": \"0\", \n\"APs\": [\n" );
         httpdSend( connData, buff, len );
         // ESP_LOGD( TAG, "%s", buff );
      }
      else
      {         // We're still scanning. Tell Javascript code that.
         ESP_LOGD( TAG, "We're still scanning" );
         len = sprintf( buff, "{\n \"result\": { \n\"inProgress\": \"1\"\n }\n}\n" );
         httpdSend( connData, buff, len );
         // ESP_LOGD( TAG, "%s", buff );
         return HTTPD_CGI_DONE;
      }
   }
   else if( cgiWifiAps.scanInProgress == SCAN_DONE )
   {
      if( pos > 0 )
      {         // scan is done, and we have a result
         if( pos <= cgiWifiAps.noAps )
         {            // Fill in json code for an access point
            // ESP_LOGD( TAG, "Fill in json code for an access point" );
            int rssi = cgiWifiAps.apData[pos - 1].rssi;

            len = sprintf( buff, "{\"essid\": \"%s\", \"bssid\": \"" MACSTR "\", \"rssi\": \"%d\", \"rssi_perc\": \"%d\", \"enc\": \"%d\", \"channel\": \"%d\"}%s\r\n",
                           cgiWifiAps.apData[pos - 1].ssid,
                           MAC2STR( cgiWifiAps.apData[pos - 1].bssid ),
                           rssi,
                           rssi2perc( rssi ),
                           cgiWifiAps.apData[pos - 1].authmode,
                           cgiWifiAps.apData[pos - 1].primary,
                           ( pos - 1 == cgiWifiAps.noAps - 1 ) ? "\r\n  " : ",\r\n   " ); // <-terminator

            httpdSend( connData, buff, len );
            // ESP_LOGD( TAG, "%s", buff );
         }

         if( pos >= cgiWifiAps.noAps )
         {            // terminate datas of previous scan and start a new scan
            len = sprintf( buff, "]\n}\n}\n" ); // terminate the whole object

            httpdSend( connData, buff, len );
            // ESP_LOGD( TAG, "%s", buff );

            // Clear ap data
            if( cgiWifiAps.apData != NULL )
            {
               free( cgiWifiAps.apData );
               cgiWifiAps.apData = NULL;
            }

            cgiWifiAps.scanInProgress = SCAN_IDLE;
            return HTTPD_CGI_DONE;
         }
         else
         {
            pos++;
            connData->cgiData = ( void* )pos;
         }
      }
   }
   return HTTPD_CGI_MORE;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// This cgi uses the routines above to connect to a specific access point with the
// given ESSID using the given password.

CgiStatus ICACHE_FLASH_ATTR cgiWiFiConnect( HttpdConnData *connData )
{
   ESP_LOGD( TAG, "cgiWiFiConnect ... " );

   char essid[128];
   char password[128];

   if( connData->isConnectionClosed )
   {
      // Connection aborted. Clean up.
      return HTTPD_CGI_DONE;
   }

   // stop scanning
   if( cgiWifiAps.scanInProgress != SCAN_IDLE )
   {
      ESP_LOGD( TAG, "cgiWiFiConnect: stop scan " );
      wifiStopScan();
      cgiWifiAps.scanInProgress = SCAN_IDLE;
      cgiWifiAps.noAps = 0;
   }

   int ssilen = httpdFindArg( connData->post.buff, "essid", essid, sizeof( essid ) );
   int passlen = httpdFindArg( connData->post.buff, "password", password, sizeof( password ) );

   if( ssilen == -1 || passlen == -1 )
   {
      ESP_LOGE( TAG, "Not rx needed args!" );
      httpdRedirect( connData, "/wifi" );
   }
   else
   {
      // get the current wifi configuration
      // check if we are in station mode
      if( ESP_OK == wifiGetConfig( WIFI_IF_STA, &wifi_config_cgi ) )
      {
         strncpy( ( char* )wifi_config_cgi.sta.ssid, essid, 32 );
         strncpy( ( char* )wifi_config_cgi.sta.password, password, 64 );
         ESP_LOGI( TAG, "Try to connect to AP \"%s\" pw \"%s\"", essid, password );

         // Set to 0 if you want to disable the actual reconnecting bit
  #ifdef DEMO_MODE
         httpdRedirect( connData, "/wifi" );
  #else
         // redirect & start connecting a little bit later
         // Schedule disconnect/connect in call back function
         connTryStatus = CONNTRY_IDLE;
         staConnectTimer = httpdPlatTimerCreate( "reconnectTimer", 1000, 0, staWiFiDoConnectCb, NULL );
         httpdPlatTimerStart( staConnectTimer );
  #endif
         httpdRedirect( connData, "/wifi/connecting.html" ); // -> "connstatus.cgi" -> cgiWiFiConnStatus()
      }
   }
   return HTTPD_CGI_DONE;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// called from web page /wifi/connecting.html --> GET connstatus.cgi -->

// json parameter
//      "status"  : "idle" | "success" | "working" | "fail"
//      "ip"

CgiStatus ICACHE_FLASH_ATTR cgiWiFiConnStatus( HttpdConnData *connData )
{
   ESP_LOGD( TAG, "cgiWiFiConnStatus: %d - %s", connTryStatus,
             connTryStatus == 0 ? "Idle"    :
             connTryStatus == 1 ? "Working" :
             connTryStatus == 2 ? "Success" :
                                  "Fail"    );
   char buff[128];
   int len;

   httpdStartResponse( connData, 200 );
   httpdHeader( connData, "Content-Type", "application/json" );
   httpdEndHeaders( connData );

   if( connTryStatus == CONNTRY_IDLE )
   {
      len = sprintf( buff, "{\n \"status\": \"idle\"\n }\n" );
   }
   else if( connTryStatus == CONNTRY_WORKING || connTryStatus == CONNTRY_SUCCESS )
   {
      int bits = xEventGroupWaitBits( wifi_event_group, WIFI_STA_CONNECTED, 0, 0, 0 );
      if( bits & WIFI_STA_CONNECTED )
      {
         tcpip_adapter_ip_info_t ipInfo;
         tcpip_adapter_get_ip_info( TCPIP_ADAPTER_IF_STA, &ipInfo );

         len = sprintf( buff, "{\"status\": \"success\", \"ip\": \""IPSTR"\"}", GOOD_IP2STR( ipInfo.ip.addr ) );
         // ESP_LOGD( TAG, "%s", buff );

         // Go to STA mode. Leave AP mode
         ESP_LOGD( TAG, "Got IP "IPSTR". Going into STA mode ...", GOOD_IP2STR( ipInfo.ip.addr ) );
         ESP_LOGD( TAG, "staCheckTimer ( 1000 )...." );
         staCheckTimer = httpdPlatTimerCreate( "staCheckTimer", 1000, 0, staWifiCheckConnStatusCb, NULL );
         httpdPlatTimerStart( staCheckTimer );
      }
      else
      {
         len = sprintf( buff, "{\n \"status\": \"working\"\n }\n" );
      }
   }
   else
   {
      len = sprintf( buff, "{\n \"status\": \"fail\"\n }\n" );
   }

   httpdSend( connData, buff, len );
   return HTTPD_CGI_DONE;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// This cgi uses the routines above to connect to a specific access point with the
// given ESSID using the given password.

CgiStatus ICACHE_FLASH_ATTR cgiWiFiSetMode( HttpdConnData *connData )
{
   int len;
   char buff[128];

   if( connData->isConnectionClosed )
   {
      // Connection aborted. Clean up.
      return HTTPD_CGI_DONE;
   }

   len = httpdFindArg( connData->getArgs, "mode", buff, sizeof( buff ) );
   if( len > 0 )
   {
      ESP_LOGD( TAG, "cgiWifiSetMode: %s", buff );
#ifndef DEMO_MODE
      wifiSetNewMode( atoi( buff ) );
#endif
   }
   httpdRedirect( connData, "/wifi" );
   return HTTPD_CGI_DONE;
}

// --------------------------------------------------------------------------
// taken from MightyPork/libesphttpd
// --------------------------------------------------------------------------

// Set wifi channel for AP mode
CgiStatus ICACHE_FLASH_ATTR cgiWiFiSetChannel( HttpdConnData *connData )
{
   int len;
   char buff[64];

   if( connData->isConnectionClosed )
   {
      // Connection aborted. Clean up.
      return HTTPD_CGI_DONE;
   }

   len = httpdFindArg( connData->getArgs, "ch", buff, sizeof( buff ) );
   if( len > 0 )
   {
      ESP_LOGD( TAG, "cgiWifiSetChannel: %s", buff );
      int channel = atoi( buff );
      if( channel > 0 && channel < 15 )
      {
         ESP_LOGD( TAG, "Setting channel=%d", channel );

         // check if we are in softap mode
         wifiSetChannel( channel, 0 );
      }
   }
   httpdRedirect( connData, "/wifi" );
   return HTTPD_CGI_DONE;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// Template code for the WLAN page.
// token:
//   %WiFiMode%
//   %WiFiApWarn%
//   %WiFiCurrSsid%
//   %WiFiPassword%
//   %wlan_net%
//   %wlan_password%

CgiStatus ICACHE_FLASH_ATTR tplWlan( HttpdConnData *connData, char *token, void **arg )
{
   char buff[256];
   wifi_config_t wifi_config = { 0 };    // ssid and password have the same location in .ap and .sta
   wifi_mode_t mode;
   esp_err_t err;

   if( token == NULL ) return HTTPD_CGI_DONE;
   strcpy( buff, "Unknown" );

   err = wifiGetCurrentMode( &mode );
   if( err != ESP_OK ) return HTTPD_CGI_DONE;

   // better to have two wifi_config variables, on for ap, one for sta
   // to cover also WiFi station + soft-AP mode
   if( mode & WIFI_MODE_AP )
   {
      wifiGetConfig( WIFI_IF_AP, &wifi_config );
      if( err != ESP_OK ) return HTTPD_CGI_DONE;

      // ESP_LOGD( TAG, "AP mode, \"%s\" \"%s\"", wifi_config.ap.ssid, wifi_config.ap.password );
   }

   if( mode & WIFI_MODE_STA )
   {
      int bits = xEventGroupWaitBits( wifi_event_group, WIFI_STA_CONNECTED, 0, 0, 0 );
      if( bits & WIFI_STA_CONNECTED )
      {
         err = wifiGetConfig( WIFI_IF_STA, &wifi_config );
         if( err != ESP_OK ) return HTTPD_CGI_DONE;

         // ESP_LOGD( TAG, "sta mode, connected \"%s\"", wifi_config.sta.ssid );
      }
      else
      {
         ESP_LOGD( TAG, "sta mode, disconnected" );
      }
   }
   else
   {
      ESP_LOGD( TAG, "NULL mode" );
   }

   if( strcmp( token, "WiFiMode" ) == 0 )
   {
      strcpy( buff, opmode2str( mode ) );
      // ESP_LOGD( TAG, "Wifi Mode: %s", buff );
   }
   else if( strcmp( token, "WiFiCurrSsid" ) == 0 )
   {
      strcpy( buff, ( char* )wifi_config.sta.ssid );
      // ESP_LOGD( TAG, "SSID: \"%s\"", buff );
   }
   else if( strcmp( token, "WiFiPassword" ) == 0 )
   {
      strcpy( buff, ( char* )wifi_config.sta.password );
      // ESP_LOGD( TAG, "Password: \"%s\"", buff );
   }
   else if( strcmp( token, "WiFiApWarn" ) == 0 )
   {
      if( mode == WIFI_MODE_AP )
      {
         strcpy( buff, "<b>Can't scan in this mode.</b> Click <a href=\"wifi/setmode.cgi?mode=3\">here</a> to go to Client+AP mode." );
      }
      else
      {
         strcpy( buff, "Click <a href=\"wifi/setmode.cgi?mode=2\">here</a> to go to standalone AP mode"
                       " or <a href=\"wifi/setmode.cgi?mode=3\">here</a> to go to Client+AP mode." );
      }
      // ESP_LOGD( TAG, "WiFiApWarn: \"%s\"", buff );
   }
   httpdSend( connData, buff, -1 );

   return HTTPD_CGI_DONE;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#endif // ESP_PLATFORM