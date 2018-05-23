// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things - Wifi Camera
//
// File          app_wifi.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
// 2018-03-26  AWe   xx
//
// --------------------------------------------------------------------------


#ifndef __APP_WIFI_H__
#define __APP_WIFI_H__

#include "esp_wifi_types.h"  // wifi_mode_t;

// --------------------------------------------------------------------------
// prototypes
// --------------------------------------------------------------------------

bool wifiConnect( void );
void wifiDisconnect( void );
void wifiStartScan( void );
void wifiStopScan( void );
uint16_t wifiScanDone( wifi_ap_record_t **ap_records );

void wifiIinitialize( void );

void wifiShowStatus( void );
void wifiStartWps( void );

void wifiSetNewMode( wifi_mode_t mode );
bool wifiGetCurrentMode( wifi_mode_t *mode );
esp_err_t wifiSetConfig( wifi_interface_t interface, wifi_config_t *conf );
esp_err_t wifiGetConfig( wifi_interface_t interface, wifi_config_t *conf );
esp_err_t wifiSetChannel( uint8_t primary, wifi_second_chan_t second );

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#endif // __APP_WIFI_H__
