// DRIVER_NVS
// APRIL 29, 2026

#ifndef _DRIVER_NVS_
#define _DRIVER_NVS_

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#define DRIVER_NVS_API_KEY_LEN_MAX      (128)
#define DRIVER_NVS_USER_ID_LEN_MAX      (64)
#define DRIVER_NVS_DEVICE_ID_LEN_MAX    (64)
#define DRIVER_NVS_REGION_LEN_MAX       (16)

typedef struct {
    char api_key[DRIVER_NVS_API_KEY_LEN_MAX];
    char user_id[DRIVER_NVS_USER_ID_LEN_MAX];
    char device_id[DRIVER_NVS_DEVICE_ID_LEN_MAX];
    char region[DRIVER_NVS_REGION_LEN_MAX];
}driver_nvs_config_t;

bool DRIVER_NVS_Init(void);

driver_nvs_config_t* DRIVER_NVS_ReadConfig(void);

#endif
