#pragma once

#include "esp_err.h"

/* Workbench SoftAP gateway (the Pi) as seen by a DUT provisioned onto the
   workbench's 192.168.4.x test network. On a real LAN the Pi is pi4b.local
   (DHCP) — override this URL at build time if the DUT reaches the Pi there. */
#define OTA_DEFAULT_URL "http://192.168.4.1:8080/firmware/test-firmware/wb-test-firmware.bin"

esp_err_t ota_update_start(void);
