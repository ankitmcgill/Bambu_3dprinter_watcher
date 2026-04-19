// DRIVER_SPIFFS
// OCTOBER 3, 2025

#ifndef _DRIVER_SPIFFS_
#define _DRIVER_SPIFFS_

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#define DRIVER_SPIFFS_FILES_MAX     (3)
#define DRIVER_SPIFFS_MOUNT_PATH    ("/spiff")


bool DRIVER_SPIFFS_Init(void);

bool DRIVER_SPIFFS_ReadFile(char* file_path, char* buffer);
void DRIVER_SPIFFS_PrintInfo(void);

#endif