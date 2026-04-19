// DEFINE COMMON DATA TYPES
// SEPTEMBER 9, 2025

#ifndef _DEFINE_COMMON_DATA_TYPES_
#define _DEFINE_COMMON_DATA_TYPES_

#define BIT_VALUE(x)    (1 << x)

typedef enum{
    COMPONENT_TYPE_NON_TASK = 0,
    COMPONENT_TYPE_TASK
}rtos_component_type_t;

#endif