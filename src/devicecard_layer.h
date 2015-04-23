#pragma once
#include "common.h"
  
// Device Card layer structure, which stores all the properties necessary for showing the device details
typedef struct {
  Layer *layer;
  DeviceType device_type;
  char location[30];
  char name[30];
  DeviceStatus status;
  char status_changed[20];
  AppTimer *animation_timer;
  uint8_t animation_step;
} DeviceCardLayer;

DeviceCardLayer* devicecard_layer_create(GRect frame);
void devicecard_layer_destroy(DeviceCardLayer *devicecard_layer);
void devicecard_layer_set_type(DeviceCardLayer *devicecard_layer, DeviceType device_type);
void devicecard_layer_set_location(DeviceCardLayer *devicecard_layer, const char *location);
void devicecard_layer_set_name(DeviceCardLayer *devicecard_layer, const char *name);
void devicecard_layer_set_status(DeviceCardLayer *devicecard_layer, DeviceStatus status);
void devicecard_layer_set_status_changed(DeviceCardLayer *devicecard_layer, const char *status_changed);
Layer* devicecard_layer_get_layer(DeviceCardLayer *devicecard_layer);