#pragma once
#include <pebble.h>
#include "common.h"

typedef void (*CommsErrorCallback)(char *error_message);
typedef void (*DeviceListCallback)();
typedef void (*DeviceDetailsCallback)(int device_id, char *location, char *name, DeviceType device_type);
typedef void (*DeviceStatusCallback)(int device_id, DeviceStatus status, char *status_changed);
typedef void (*DeviceStatusSetCallback)(int device_id);

void init_comms();
void comms_register_errorhandler(CommsErrorCallback callback);
void comms_register_devicelist(DeviceListCallback callback);
void comms_register_devicedetails(DeviceDetailsCallback callback);
void comms_register_devicestatus(DeviceStatusCallback callback);
void comms_register_devicestatusset(DeviceStatusSetCallback callback);
void device_list_fetch();
void device_details_fetch(int device_id);
void device_status_fetch(int device_id);
void device_status_set(int device_id, DeviceStatus status);