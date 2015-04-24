#pragma once
#include <pebble.h>
  
//#define DEBUG
#ifndef DEBUG
#undef APP_LOG
#define APP_LOG(...)
#endif

// Types of MyQ devices (not the same IDs as the MyQ JSON. Instead must match IDs in main.js)
typedef enum DeviceType {
  DTUnknown = 0,
  DTGarageDoor = 1,
  DTLightSwitch = 2,
  DTGate = 3
} DeviceType;

// MyQ device statuses (only tested with Garage Doors and negative values have been added)
typedef enum DeviceStatus {
  DSLoading = -2,
  DSUpdating = -1,
  DSOnOpen = 1,
  DSOffClosed = 2,
  DSOpening = 4,
  DSClosing = 5,
  DSVGDOOpen = 9
} DeviceStatus;

// Global variables
extern int *g_device_id_list; // Will be allocated as an array when passed from phone
extern int g_device_count;
extern int g_device_selected;
