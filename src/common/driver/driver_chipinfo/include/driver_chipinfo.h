// DRIVER_CHIPINFO
// AUGUST 26, 2025

#ifndef _DRIVER_CHIPINFO_
#define _DRIVER_CHIPINFO_

#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_flash.h"
#include "esp_mac.h"

bool DRIVER_CHIPINFO_Init(void);

bool DRIVER_CHIPINFO_GetChipInfo(esp_chip_info_t *info);
bool DRIVER_CHIPINFO_GetChipID(uint8_t* val);
uint32_t DRIVER_CHIPINFO_GetFlashSizeBytes(void);
uint32_t DRIVER_CHIPINFO_GetRamSizeBytes(void);

#endif