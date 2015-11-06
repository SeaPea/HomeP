#include <pebble.h>
#include "comms.h"

// Unit that contains all functionality for communicating with the phone JS

// JS App Message keys
#define FUNCTION_KEY 0
#define ERROR_MESSAGE 1
#define DEVICE_LIST 2
#define DEVICE_ID 3
#define DEVICE_LOCATION 4
#define DEVICE_NAME 5
#define DEVICE_TYPE 6
#define DEVICE_STATUS 7
#define STATUS_CHANGED 8

// List of message types (function keys - FK)
#define FK_ERROR -1
#define FK_LIST_DEVICES 1
#define FK_GET_DEVICE_DETAILS 2
#define FK_GET_DEVICE_STATUS 3
#define FK_SET_DEVICE_STATUS 4

// Callbacks for signalling inbound comms
static CommsErrorCallback s_callback_error = NULL;
static DeviceListCallback s_callback_devicelist = NULL;
static DeviceDetailsCallback s_callback_devicedetails = NULL;
static DeviceStatusCallback s_callback_devicestatus = NULL;
static DeviceStatusSetCallback s_callback_devicestatusset = NULL;

// Structure for passing single device details
typedef struct devicedetails_t {
  int device_id;
  char location[30];
  char name[30];
  DeviceType device_type;
} devicedetails_t;

// Structure for passing device status
typedef struct devicestatus_t {
  int device_id;
  int8_t status;
  char status_changed[20];
} devicestatus_t;

// Timer delayed callback for error messages from the JS or App Message errors
// (Timers are used to call back to a listener after the App Message subsystem has cleared the buffer
//  so that chained messaged do not cause busy errors)
void callback_error_delayed(void *data) {
  if (data != NULL) {
    if (s_callback_error != NULL) s_callback_error((char*)data);
    free(data);
  }
}

// Timer delayed callback for receiving the device ID list that is stored in a global variable
void callback_devicelist_delayed(void *data) {
  if (s_callback_devicelist != NULL) s_callback_devicelist();
}

// Timer delayed callback for receiving single device details
void callback_devicedetails_delayed(void *data) {
  if (data != NULL) {
    devicedetails_t *details = data;
    if (s_callback_devicedetails != NULL) 
      s_callback_devicedetails(details->device_id, details->location, details->name, details->device_type);
    free(data);
  }
}

// Timer delayed callback for receiving device status
void callback_devicestatus_delayed(void *data) {
  if (data != NULL) {
    devicestatus_t *status = data;
    if (s_callback_devicestatus != NULL) 
      s_callback_devicestatus(status->device_id, status->status, status->status_changed);
    free(data);
  }
}

// Timer delayed callback for receiving status set confirmation
void callback_devicestatusset_delayed(void *data) {
  if (data != NULL) {
    int *device_id = data;
    if (s_callback_devicestatusset != NULL)
      s_callback_devicestatusset(*device_id);
    free(data);
  }
}

// Show an error to the user using the error callback
void show_error(char *error) {
  char *error_message = malloc(100);
  strncpy(error_message, error, 100); 
  error_message[99] = '\0';
  if (s_callback_error != NULL) app_timer_register(100, callback_error_delayed, error_message);
}

// Received comms from JS
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Get the function key that defines the message type
  Tuple *t_func = dict_find(iterator, FUNCTION_KEY);
  
  Tuple *t_error = NULL;
  Tuple *t_id_list = NULL;
  Tuple *t_device_id = NULL;
  Tuple *t_location = NULL;
  Tuple *t_name = NULL;
  Tuple *t_type = NULL;
  Tuple *t_status = NULL;
  Tuple *t_status_changed = NULL;
  char msg[50];
  
  if (t_func != NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Inbox Rx - Function: %d", t_func->value->int16);
    switch (t_func->value->int16) {
      case FK_ERROR:
        // Error from JS
        t_error = dict_find(iterator, ERROR_MESSAGE);
        if (t_error != NULL) {
          show_error(t_error->value->cstring);
        } else {
          show_error("Error comms missing error message");
        }
        break;
      
      case FK_LIST_DEVICES:
        // Find and parse device list, which is sent as a byte array of int
        t_id_list = dict_find(iterator, DEVICE_LIST);
        if (t_id_list != NULL) {
          if (g_device_id_list != NULL) free(g_device_id_list);
          g_device_id_list = malloc(t_id_list->length);
          memcpy(g_device_id_list, t_id_list->value->data, t_id_list->length);
          g_device_count = t_id_list->length / 4;
          
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Device Count: %d", g_device_count);
          for (int i = 0; i < g_device_count; i++)
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Device %d: %d", i, (int)g_device_id_list[i]);
          
          // Callback to signal device list received after a short delay so that this proc can exit
          // before the app sends another message
          if (s_callback_devicelist != NULL) app_timer_register(100, callback_devicelist_delayed, NULL);
        } else {
          show_error("List Devices comms missing device ID list");
        }
        break;
      
      case FK_GET_DEVICE_DETAILS:
        // Recieved device details in multiple key/value pairs
        t_device_id = dict_find(iterator, DEVICE_ID);
        t_location = dict_find(iterator, DEVICE_LOCATION);
        t_name = dict_find(iterator, DEVICE_NAME);
        t_type = dict_find(iterator, DEVICE_TYPE);
        if (t_device_id != NULL && t_location != NULL && t_name != NULL && t_type != NULL) {
          if (s_callback_devicedetails != NULL) {
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Device ID Rx: %d", (int)t_device_id->value->int32);
            devicedetails_t *details = malloc(sizeof(devicedetails_t));
            details->device_id = (int)t_device_id->value->int32;
            strncpy(details->location, t_location->value->cstring, sizeof(details->location));
            details->location[sizeof(details->location)-1] = '\0';
            strncpy(details->name, t_name->value->cstring, sizeof(details->name));
            details->name[sizeof(details->name)-1] = '\0';
            details->device_type = t_type->value->uint8;
            
            // Callback to signal device details received after a short delay so that this proc can exit
            // before the app sends another message
            app_timer_register(100, callback_devicedetails_delayed, details);
          }
        } else {
          show_error("Device Details comms missing a parameter");
        }
        break;
      
      case FK_GET_DEVICE_STATUS:
        // Received device status
        t_device_id = dict_find(iterator, DEVICE_ID);
        t_status = dict_find(iterator, DEVICE_STATUS);
        if (t_device_id != NULL && t_status != NULL) {
          if (s_callback_devicestatus != NULL) {
            devicestatus_t *status = malloc(sizeof(devicestatus_t));
            status->device_id = (int)t_device_id->value->int32;
            status->status = t_status->value->int8;
            t_status_changed = dict_find(iterator, STATUS_CHANGED);
            if (t_status_changed == NULL) {
              strcpy(status->status_changed, "");
            } else {
              strncpy(status->status_changed, t_status_changed->value->cstring, sizeof(status->status_changed));
              status->status_changed[sizeof(status->status_changed)-1] = '\0';
            }
            
            // Callback to signal device status received after a short delay so that this proc can exit
            // before the app sends another message
            app_timer_register(100, callback_devicestatus_delayed, status);
          }
        } else {
          show_error("Get Device Status comms missing parameter");
        }
        break;
      
      case FK_SET_DEVICE_STATUS:
        // JS has indicated that the server received the new status
        t_device_id = dict_find(iterator, DEVICE_ID);
        if (t_device_id != NULL && s_callback_devicestatusset != NULL) {
          int *device_id = malloc(sizeof(int));
          *device_id = (int)t_device_id->value->int32;
          
          app_timer_register(100, callback_devicestatusset_delayed, device_id);
        } else {
          show_error("Set Devices Status comms missing parameter");
        }
        break;
      
      default:
        snprintf(msg, sizeof(msg), "Unknown comms message: %d", t_func->value->int16);
        show_error(msg);
    }
  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Inbox Rx - NULL Function!"); 
    show_error("Missing comms function");
  }
  
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
  char msg[100];
  snprintf(msg, sizeof(msg), "Inbound message dropped: %d. Please restart the app", reason);
  show_error(msg);
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
  char msg[100];
  snprintf(msg, sizeof(msg), "Outbound message failed: %d. Please restart the app", reason);
  show_error(msg);
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

void init_comms() {
  // Register App Message callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  // Open App Message
  app_message_open((app_message_inbox_size_maximum() < 2048 ? app_message_inbox_size_maximum() : 2048), 
                   APP_MESSAGE_OUTBOX_SIZE_MINIMUM);
}

void comms_register_errorhandler(CommsErrorCallback callback) {
  s_callback_error = callback;
}

void comms_register_devicelist(DeviceListCallback callback) {
  s_callback_devicelist = callback;
}

void comms_register_devicedetails(DeviceDetailsCallback callback) {
  s_callback_devicedetails = callback;
}

void comms_register_devicestatus(DeviceStatusCallback callback) {
  s_callback_devicestatus = callback;
}

void comms_register_devicestatusset(DeviceStatusSetCallback callback) {
  s_callback_devicestatusset = callback;
}

// Send request to list devices
void device_list_fetch() {
  // Setup tuplets for function to phone
  Tuplet t_func = TupletInteger(FUNCTION_KEY, FK_LIST_DEVICES);
  
  // Put dictionary together
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);

  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Send iter is NULL");
    char msg[100];
    snprintf(msg, sizeof(msg), "Device list comms error: %d. Please restart app.", result);
    show_error(msg);
    return;
  }
  
  dict_write_tuplet(iter, &t_func);
  dict_write_end(iter);
  
  // Send to phone
  app_message_outbox_send();
}

// Send request to get device details
void device_details_fetch(int device_id) {
  // Setup tuplets for function to phone
  Tuplet t_func = TupletInteger(FUNCTION_KEY, FK_GET_DEVICE_DETAILS);
  Tuplet t_device_ID = TupletInteger(DEVICE_ID, device_id);
  
  // Put dictionary together
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);

  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Send iter is NULL");
    char msg[100];
    snprintf(msg, sizeof(msg), "Device details comms error: %d. Please restart app.", result);
    show_error(msg);
    return;
  }
  
  dict_write_tuplet(iter, &t_func);
  dict_write_tuplet(iter, &t_device_ID);
  dict_write_end(iter);
  
  // Send to phone
  app_message_outbox_send();
}

// Send request to get device status
void device_status_fetch(int device_id) {
  // Setup tuplets for function to phone
  Tuplet t_func = TupletInteger(FUNCTION_KEY, FK_GET_DEVICE_STATUS);
  Tuplet t_device_ID = TupletInteger(DEVICE_ID, device_id);
  
  // Put dictionary together
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);

  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Send iter is NULL - Result: %d", result);
    char msg[100];
    snprintf(msg, sizeof(msg), "Device status fetch comms error: %d. Please restart app.", result);
    show_error(msg);
    return;
  }
  
  dict_write_tuplet(iter, &t_func);
  dict_write_tuplet(iter, &t_device_ID);
  dict_write_end(iter);
  
  // Send to phone
  app_message_outbox_send();
}

// Send request to set device status
void device_status_set(int device_id, DeviceStatus status) {
  // Setup tuplets for function to phone
  Tuplet t_func = TupletInteger(FUNCTION_KEY, FK_SET_DEVICE_STATUS);
  Tuplet t_device_ID = TupletInteger(DEVICE_ID, device_id);
  Tuplet t_status = TupletInteger(DEVICE_STATUS, status);
  
  // Put dictionary together
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);

  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Send iter is NULL");
    char msg[100];
    snprintf(msg, sizeof(msg), "Device status set comms error: %d. Please restart app.", result);
    show_error(msg);
    return;
  }
  
  dict_write_tuplet(iter, &t_func);
  dict_write_tuplet(iter, &t_device_ID);
  dict_write_tuplet(iter, &t_status);
  dict_write_end(iter);
  
  // Send to phone
  app_message_outbox_send();
}