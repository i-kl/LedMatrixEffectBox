#pragma once

#define OTA_PASSWORD										"PROVIDE YOUR PASSWORD FOR OTA UPDATE"

// router settings (can be ignored if OTA is disabled)
#define ROUTER_SSID											"SSID OF YOUR ROUTER"
#define ROUTER_PASSWD										"PASSWORD OF YOUR ROUTER"
#define PRODUCT_NAME										"LedMatrixEffectBox_"
#define MAX_NUM_OF_WIFI_SSID_TRIAL_TIMEOUT_IN_500MS			10 // 6 sec
const IPAddress G_ownStaticIP								= IPAddress(192, 168, 8, 205);	// UPDATE IF NEEDED
const IPAddress G_gatewayIP									= IPAddress(192, 168, 8, 1);	// UPDATE IF NEEDED
const IPAddress G_maskIP									= IPAddress(255, 255, 0, 0);	// UPDATE IF NEEDED
