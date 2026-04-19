// DRIVER_APPINFO
// AUGUST 26, 2025

#ifndef _DRIVER_APPINFO_
#define _DRIVER_APPINFO_

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "esp_mac.h"
#include "esp_idf_version.h"
#include "esp_app_desc.h"

bool DRIVER_APPINFO_Init(void);

bool DRIVER_APPINFO_GetProjectName(char* str);
bool DRIVER_APPINFO_GetCompileDateTime(char* str);
bool DRIVER_APPINFO_GetGitDetails(char* str);
bool DRIVER_APPINFO_GetIDFVersion(char* str);

#endif