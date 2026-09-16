#ifndef _PROFILE_H
#define _PROFILE_H

// Build profile defaults only. Runtime hardware pinout lives in device_config[].

#ifdef ULTRA

  #define PROFILE             "[ULTRA]"
  #define _HOSTNAME           "Ultra-Dongle"
  #define _HOTSPOT            "P1-Dongle-Ultra"
  #define _DEFAULT_HOSTNAME   _HOSTNAME
  #define OTA_HW_ID           "p1u/"

  #define ETHERNET
  #define VIRTUAL_P1
  #define NETSWITCH
  #define MB_RTU
  #define RTU_SERIAL          Serial2

#elif defined(ETHERNET)

  #define NETSWITCH

  #ifdef ETH_P1EP
    #define PROFILE           "[P1EP]"
    #define VIRTUAL_P1
    #define OTA_HW_ID         "p1ep/"
  #else
    #define PROFILE           "[ETH]"
    #define DTR_IO            0
    #define BRIGHTNESS        100
    #define OTA_HW_ID         "p1e/"
  #endif

  #define _HOSTNAME           "Eth-Dongle-Pro"
  #define _DEFAULT_HOSTNAME   _HOSTNAME

#elif defined(NRG_DONGLE)

  #define PROFILE             "[NRGD]"
  #define _HOSTNAME           "NRG-Dongle-Pro"
  #define _HOTSPOT            "NRG-Dongle-Pro"
  #define _DEFAULT_HOSTNAME   _HOSTNAME

  #define MB_RTU
  #define RTU_SERIAL          Serial
  #define OTA_HW_ID           "nrgd/"

#else

  #define PROFILE             "[P1P]"
  #define _HOSTNAME           "P1-Dongle-Pro"
  #define _HOTSPOT            "P1-Dongle-Pro"
  #define _DEFAULT_HOSTNAME   _HOSTNAME
  #define OTA_HW_ID           "p1p/"

  #define DTR_IO              6

#endif

#ifdef __Az__
  #undef _HOSTNAME
  #undef _HOTSPOT
  #undef _DEFAULT_HOSTNAME
  #ifdef ETHERNET
    #define _HOSTNAME         "azimut-p1-ethernet"
  #else
    #define _HOSTNAME         "azimut-p1-wifi"
    #define _HOTSPOT          "azimut-p1-hotspot"
  #endif
  #define _DEFAULT_HOSTNAME   _HOSTNAME
#endif

#ifndef OTAURL_PREFIX
  #if DIRECT_AP_CONNECT
    #define OTAURL_PREFIX     DIRECT_AP_OTAURL_PREFIX
  #elif defined(POST_POWERCH)
    #define OTAURL_PREFIX     "pc/"
  #else
    #define OTAURL_PREFIX     ""
  #endif
#endif

#ifndef OTA_BASE_URL
  #ifdef ULTRA
    #define OTA_BASE_URL      "http://209.38.55.197/p1dongle/ultra/"
  #else
    #define OTA_BASE_URL      "http://ota.smart-stuff.nl/"
  #endif
#endif

#ifndef OTA_VERSION
  #ifdef ULTRA
    #define OTA_VERSION       ""
  #elif _VERSION_MAJOR == 5
    #define OTA_VERSION       "v5/"
  #else
    #define OTA_VERSION       ""
  #endif
#endif

#ifdef ULTRA
  #define OTAURL              OTA_BASE_URL
#else
  #define OTAURL              OTA_BASE_URL OTA_HW_ID OTA_VERSION
#endif

#ifndef BASE_OTA_URL_SIZE
  #define BASE_OTA_URL_SIZE   96
#endif

#ifndef IO_BUTTON
  #define IO_BUTTON           -1
#endif
#ifndef RGBLED_PIN
  #define RGBLED_PIN          -1
#endif
#ifndef BRIGHTNESS
  #define BRIGHTNESS          50
#endif
#ifndef DTR_IO
  #define DTR_IO              -1
#endif
#ifndef RXP1
  #define RXP1                -1
#endif
#ifndef TXO1
  #define TXO1                -1
#endif
#ifndef O1_DTR_IO
  #define O1_DTR_IO           -1
#endif
#ifndef P1_LED
  #define P1_LED              -1
#endif
#ifndef LED
  #define LED                 -1
#endif
#ifndef RXPIN
  #define RXPIN               -1
#endif
#ifndef TXPIN
  #define TXPIN               -1
#endif
#ifndef RTSPIN
  #define RTSPIN              -1
#endif

#endif
