#ifndef VE_COMMON_H
#define VE_COMMON_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#define VERSION     1
#define VERSION_STR "1.0.0"
#define DEVICE_MAX 50
#define SIZE_NAME 64
#define MAXDRIVERS 10
#define MAX_CHANNEL_COUNT 100
#define DBUS_FIELDS_PER_CHANNEL     3
#define DEFAULT_PROBE_INTERVAL      1500
#define OFFLINE_PROBE_INTERVAL      30000

typedef struct
{
    const char* path;
    int type;
    double default_num;
    const char* default_str;
    double output_dec;
    uint32_t output_uint;
    char* output_str;
    uint16_t id;            // assigned at startup
} ve_dbus_path_t;

typedef struct 
{
    char* ve_key;
    char* channel_name;
    ve_dbus_path_t* dbus_ptr;
} yasdi_bridge_keymap_t;

#endif