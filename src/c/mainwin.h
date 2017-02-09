#include "common.h"

typedef void (*UIDeviceSwitchedCallback)();
typedef void (*UIStatusChangeCallback)();


void ui_register_deviceswitch(UIDeviceSwitchedCallback callback);
void ui_register_statuschange(UIStatusChangeCallback callback);

void show_device_count();
void show_device_details(const char *location, const char *name, const DeviceType device_type);
void show_device_status(const DeviceStatus status, const char *status_changed);

void show_mainwin(void);
void hide_mainwin(void);
bool showing_mainwin(void);