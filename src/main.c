#include <pebble.h>
#include "mainwin.h"
#include "comms.h"
#include "msg.h"

// Main application unit

// Global variables
int *g_device_id_list; // Will be allocated as an array when passed from phone
int g_device_count;
int g_device_selected;

// Static unit variables
static DeviceStatus s_device_status_target = 0;
static DeviceStatus s_device_status = 0;
static char s_status_changed[20] = "";
static AppTimer *inactivity_timer = NULL;
static AppTimer *status_change_timeout_timer = NULL;
static AppTimer *status_change_check_timer = NULL;

// Close the app after a period of inactivity (to prevent accidentally operating devices)
void inactivity_timeout(void *data) {
  inactivity_timer = NULL;
  window_stack_pop_all(true);
}

// Set/Reset inactivity timer to close app after 2 minutes
void reset_inactivity_timer() {
  if (inactivity_timer == NULL)
    inactivity_timer = app_timer_register(120000, inactivity_timeout, NULL);
  else
    app_timer_reschedule(inactivity_timer, 120000);
}

// Cancel the timeout timer that is started when changing a device's status
void cancel_timeout() {
  if (status_change_timeout_timer != NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Cancelling timeout");
    app_timer_cancel(status_change_timeout_timer);
    status_change_timeout_timer = NULL;
  }
}

// Cancel the status check timer that periodically checks a device's status when changing until it has changed
void cancel_status_check() {
  if (status_change_check_timer != NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Cancelling check timer");
    app_timer_cancel(status_change_check_timer);
    status_change_check_timer = NULL;
  }
}

// Callback to show any error received from the phone JS or MyQ servers
void comms_error(char *error_message) {
  cancel_status_check();
  cancel_timeout();
  show_msg(error_message, false, 0);
  if (g_device_count == 0) {
    if (inactivity_timer != NULL) {
      app_timer_cancel(inactivity_timer);
      inactivity_timer = NULL;
    }
    // If there are no devices, hide main win so the app closes when the error message is closed
    hide_mainwin();
  } else {
    reset_inactivity_timer();
  }
}

// Callback for when Device ID list has been fetched
void device_list_fetched() {
  reset_inactivity_timer();
  if (g_device_count == 0) {
    show_msg("No devices found!", false, 0);
    hide_mainwin();
  } else {
    if (!showing_mainwin()) show_mainwin();
    hide_msg();
    g_device_selected = 0;
    device_details_fetch(g_device_id_list[g_device_selected]);
  }
}

// Callback for when device details have been fetched
void device_details_fetched(int device_id, char *location, char *name, DeviceType device_type) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Details fetched - ID: %d, Location: %s, Name: %s, DeviceType: %d", 
          device_id, location, name, device_type);
  reset_inactivity_timer();
  
  if (g_device_id_list[g_device_selected] == device_id) {
    show_device_details(location, name, device_type);
    s_device_status = DSUpdating;
    show_device_status(DSUpdating, "");
    device_status_fetch(g_device_id_list[g_device_selected]);
  }
}

// Timer event when status update operation times out
void status_change_timeout(void *data) {
  reset_inactivity_timer();
  status_change_timeout_timer = NULL;
  s_device_status_target = 0;
  cancel_status_check();
  show_device_status(s_device_status, s_status_changed);
  show_msg("Operation timed out", false, 5);
}

// Timer event to update the device status periodically when it is changing
void status_change_check(void *data) {
  reset_inactivity_timer();
  status_change_check_timer = NULL;
  device_status_fetch(g_device_id_list[g_device_selected]);
}

// Callbck for when the device status has been fetched
void device_status_fetched(int device_id, DeviceStatus status, char *status_changed) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Status fetched - ID: %d, Status: %d, Selected ID: %d", 
          device_id, status, g_device_id_list[g_device_selected]);
  reset_inactivity_timer();
  
  if (g_device_id_list[g_device_selected] == device_id) {
    if (s_device_status_target != 0) {
      // If expecting the device status to change (e.g. Garage door opening/closing)
      cancel_status_check();
      
      if (((status == DSVGDOOpen) ? DSOnOpen : status) != s_device_status_target) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Status still not reached target, checking again in 2 seconds...");
        // Still not reached target status, so check again after 2 seconds
        status_change_check_timer = app_timer_register(2000, status_change_check, NULL);
      } else {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Status has reached target");
        // Status changed, so stop checking
        cancel_timeout();
        s_device_status_target = 0;
        s_device_status = status;
        strncpy(s_status_changed, status_changed, sizeof(s_status_changed));
        s_status_changed[sizeof(s_status_changed)-1] = '\0';
        show_device_status(status, status_changed);
        light_enable_interaction();
        vibes_short_pulse();
      }
      
    } else {
      // Not expecting status to change, so just update the display
      cancel_status_check();
      
      if (status != s_device_status) {
        s_device_status = status;
        light_enable_interaction();
      }
      strncpy(s_status_changed, status_changed, sizeof(s_status_changed));
      s_status_changed[sizeof(s_status_changed)-1] = '\0';
      show_device_status(status, status_changed);
    }
  }
}

// Callback for when user switches between devices
void device_switched() {
  reset_inactivity_timer();
  cancel_status_check();
  cancel_timeout();
  device_details_fetch(g_device_id_list[g_device_selected]);
}

// Callback when user indicates status should be changed
void device_status_change() {
  reset_inactivity_timer();
  switch (s_device_status) {
    case DSOnOpen:
    case DSVGDOOpen:
      s_device_status_target = DSOffClosed;
      show_device_status(DSClosing, "");
      break;
    case DSOffClosed:
      s_device_status_target = DSOnOpen;
      show_device_status(DSOpening, "");
      break;
    case DSOpening:
      s_device_status_target = DSOffClosed;
      show_device_status(DSClosing, "");
      break;
    case DSClosing:
      s_device_status_target = DSOnOpen;
      show_device_status(DSOpening, "");
      break;
    default:
      // Do nothing
      return;
      break;
  }
  // Send request to MyQ servers to change the status
  device_status_set(g_device_id_list[g_device_selected], s_device_status_target);
  status_change_timeout_timer = app_timer_register(60000, status_change_timeout, NULL);
}

// Callback for when the phone JS indicates the status change was sent to the MyQ server
void device_status_change_sent(int device_id) {
  reset_inactivity_timer();
  if (s_device_status_target != 0 && g_device_id_list[g_device_selected] == device_id) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Status change sent. Checking for status updates...");
    // Start checking for the status reaching the target 
    // (For garage doors, wait 10 seconds before first check due to how long it take)
    if (status_change_check_timer != NULL)
      app_timer_reschedule(status_change_check_timer, 10000);
    else
      status_change_check_timer = app_timer_register(10000, status_change_check, NULL);
  }
}

void handle_init(void) {
  g_device_id_list = NULL;
  show_mainwin();
  show_msg("HomeP\n\nLoading...", true, 0);
  comms_register_errorhandler(comms_error);
  comms_register_devicelist(device_list_fetched);
  comms_register_devicedetails(device_details_fetched);
  comms_register_devicestatus(device_status_fetched);
  comms_register_devicestatusset(device_status_change_sent);
  ui_register_deviceswitch(device_switched);
  ui_register_statuschange(device_status_change);
  reset_inactivity_timer();
  // Initializing comms will trigger phone JS to fetch device list
  init_comms();
}

void handle_deinit(void) {
  hide_mainwin();
  if (g_device_id_list != NULL) {
    free(g_device_id_list);
    g_device_id_list = NULL;
  }
  cancel_timeout();
  cancel_status_check();
  if (inactivity_timer != NULL) app_timer_cancel(inactivity_timer);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
