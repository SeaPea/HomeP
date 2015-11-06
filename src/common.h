#pragma once
#include <pebble.h>
  
//#define DEBUG
#ifndef DEBUG
#undef APP_LOG
#define APP_LOG(...)
#endif

#ifdef PBL_COLOR
#define IF_COLOR(statement)   (statement)
#define IF_BW(statement)
#define IF_COLORBW(color, bw) (color)
#define COLOR_SCREEN 1
#else
#define IF_COLOR(statement)
#define IF_BW(statement)    (statement)
#define IF_COLORBW(color, bw) (bw)
#define COLOR_SCREEN 0
#endif

#ifdef PBL_SDK_2
#define IF_32(sdk3, sdk2) (sdk2)
#define IF_3(sdk3)
#define IF_2(sdk2) (sdk2)
#else
#define IF_32(sdk3, sdk2) (sdk3)
#define IF_3(sdk3) (sdk3)
#define IF_2(sdk2)
#endif

#ifdef PBL_RECT
#undef ACTION_BAR_WIDTH
#define ACTION_BAR_WIDTH 20
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
  DSNone = -99,
  DSTurningOff = -4,
  DSTurningOn = -3,
  DSLoading = -2,
  DSUpdating = -1,
  DSOff = 0,
  DSOnOpen = 1,
  DSClosed = 2,
  DSOpening = 4,
  DSClosing = 5,
  DSVGDOOpen = 9
} DeviceStatus;

// Global variables
extern int *g_device_id_list; // Will be allocated as an array when passed from phone
extern int g_device_count;
extern int g_device_selected;
