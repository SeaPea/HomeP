#include <pebble.h>
#include "devicecard_layer.h"

// Custom layer that draws a 'device card', storing and displaying all the details of a 
// device including an icon.
// This has been implemented as a layer so that it can be easily animated when switching between devices.
  
// Gets a textual description of a device status
static void get_status_desc(DeviceType device_type, DeviceStatus status, char *status_desc) {
  switch (status) {
    case DSLoading:
      strcpy(status_desc, "Loading...");
      break;
    case DSUpdating:
      strcpy(status_desc, "Updating...");
      break;
    case DSOnOpen:
    case DSVGDOOpen:
      switch (device_type) {
        case DTGarageDoor:
        case DTGate:
          strcpy(status_desc, "OPEN");
          break;
        case DTLightSwitch:
          strcpy(status_desc, "ON");
          break;
        default:
          break;
      }
      break;
    case DSOff:
      strcpy(status_desc, "OFF");
      break;
    case DSClosed:
      strcpy(status_desc, "CLOSED");
      break;
    case DSOpening:
      strcpy(status_desc, "Opening...");
      break;
    case DSClosing:
      strcpy(status_desc, "Closing...");
      break;
    case DSTurningOff:
      strcpy(status_desc, "Turning Off...");
      break;
    case DSTurningOn:
      strcpy(status_desc, "Turning On...");
      break;
    default:
      strcpy(status_desc, "Status Unknown");
      break;
  }
}

// Timer event that fires when the icon is being animated
static void animate_icon(void *data) {
  if (data != NULL) {
    DeviceCardLayer *devicecard_layer = data;
    devicecard_layer->animation_timer = NULL;
    switch (devicecard_layer->device_type) {
      case DTGarageDoor:
        // When opening or closing a garage door, increment or decrement an animation step
        // and restart the timer before marking the layer dirty. The update proc will then
        // draw the garage door according to the animation step
        switch (devicecard_layer->status) {
          case DSOpening:
            devicecard_layer->animation_step = (devicecard_layer->animation_step + 5) % 6;
            if (devicecard_layer->animation_step == 5)
              devicecard_layer->animation_timer = app_timer_register(1000, animate_icon, data);
            else
              devicecard_layer->animation_timer = app_timer_register(500, animate_icon, data);
            layer_mark_dirty(devicecard_layer->layer);
            break;
          case DSClosing:
            devicecard_layer->animation_step = (devicecard_layer->animation_step + 1) % 6;
            if (devicecard_layer->animation_step == 0)
              devicecard_layer->animation_timer = app_timer_register(1000, animate_icon, data);
            else
              devicecard_layer->animation_timer = app_timer_register(500, animate_icon, data);
            layer_mark_dirty(devicecard_layer->layer);
            break;
          default:
            break;
        }
        break;
      default:
        break;
    }
  }
}

// Device card update proc that draws the whole layer
static void devicecard_layer_update_proc(Layer *layer, GContext *ctx) {
  
  // Get reference to DeviceCardLayer and its properties
  DeviceCardLayer *devicecard_layer = (DeviceCardLayer*)(layer_get_data(layer));
  
  // Draw Border and background
  GRect rect = layer_get_frame(layer);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(1, 1, rect.size.w-2, rect.size.h-2), 10, GCornersAll);
  graphics_draw_round_rect(ctx, GRect(1, 1, rect.size.w-2, rect.size.h-2), 10);
  
  graphics_context_set_text_color(ctx, GColorWhite);
  
  // Draw location text
  graphics_draw_text(ctx, devicecard_layer->location, fonts_get_system_font(FONT_KEY_GOTHIC_14), 
                     GRect(2, 2, rect.size.w-4, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // Draw name text
  graphics_draw_text(ctx, devicecard_layer->name, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), 
                     GRect(2, 14, rect.size.w-4, 20), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // Draw status text
  char status_desc[30] = "";
  get_status_desc(devicecard_layer->device_type, devicecard_layer->status, status_desc);
  graphics_draw_text(ctx, status_desc, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), 
                     GRect(2, rect.size.h-36, rect.size.w-4, 20), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // Draw text indicating when the device status last changed
  graphics_draw_text(ctx, devicecard_layer->status_changed, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(2, rect.size.h-22, rect.size.w-4, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  
  // Draw device icon showing On/Open or Off/Closed
  GBitmap *device_icon = NULL;
  
  switch (devicecard_layer->device_type) {
    case DTGarageDoor:
    
      switch (devicecard_layer->status) {
        case DSOnOpen:
        case DSVGDOOpen:
          device_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_GARAGE_OPEN);
          break;
        case DSClosed:
          device_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_GARAGE_CLOSED);
          break;
        case DSOpening:
        case DSClosing:
          switch (devicecard_layer->animation_step) {
            case 0:
              device_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_GARAGE_OPEN);
              break;
            case 1:
              device_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_GARAGE_STEP1);
              break;
            case 2:
              device_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_GARAGE_STEP2);
              break;
            case 3:
              device_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_GARAGE_STEP3);
              break;
            case 4:
              device_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_GARAGE_STEP4);
              break;
            default:
              device_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_GARAGE_CLOSED);
              break;
          }
          break;
        default:
          // Do not draw an icon for other statuses
          break;
      }
      
      break;    
    
    case DTLightSwitch:
      
      switch (devicecard_layer->status) {
        case DSOnOpen:
        case DSTurningOff:
          device_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_LIGHTBULB_ON);
          break;
        case DSOff:
        case DSTurningOn:
          device_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_LIGHTBULB_OFF);
          break;
        default:
          // Do not draw an icon for other statuses
          break;
      }
    
      break;
    
    default:
      // Only Garage Doors & Light Switches supported for now
      break;
  }
      
  if (device_icon != NULL) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    GRect icon_bounds = IF_32(gbitmap_get_bounds(device_icon), device_icon->bounds);
    graphics_draw_bitmap_in_rect(ctx, device_icon, 
                                 GRect((rect.size.w/2)-(icon_bounds.size.w/2), 31,
                                       icon_bounds.size.w, icon_bounds.size.h));
    gbitmap_destroy(device_icon);
  }
  
}
  
// Create DeviceCard layer
DeviceCardLayer* devicecard_layer_create(GRect frame) {
  
  // Create base layer with space for our data
  Layer *layer = layer_create_with_data(frame, sizeof(DeviceCardLayer));
  DeviceCardLayer *devicecard_layer = (DeviceCardLayer*)layer_get_data(layer);
  memset(devicecard_layer, 0, sizeof(DeviceCardLayer));
  
  // Init properties
  devicecard_layer->layer = layer;
  devicecard_layer->device_type = DTUnknown;
  strcpy(devicecard_layer->location, "");
  strcpy(devicecard_layer->name, "");
  devicecard_layer->status = DSLoading;
  strcpy(devicecard_layer->status_changed, "");
  devicecard_layer->animation_timer = NULL;
  devicecard_layer->animation_step = 0;
  
  layer_set_update_proc(layer, devicecard_layer_update_proc);
  
  return devicecard_layer;                    
}

// Destroy DeviceCard layer
void devicecard_layer_destroy(DeviceCardLayer *devicecard_layer) {
  if (devicecard_layer != NULL) {
    if (devicecard_layer->animation_timer != NULL) {
      app_timer_cancel(devicecard_layer->animation_timer);
      devicecard_layer->animation_timer = NULL;
    }
    if (devicecard_layer->layer != NULL) {
      layer_destroy(devicecard_layer->layer);
      devicecard_layer->layer = NULL;
    }
  }
}

// Returns base layer
Layer* devicecard_layer_get_layer(DeviceCardLayer *devicecard_layer){
  return devicecard_layer->layer;
}

// Sets device type
void devicecard_layer_set_type(DeviceCardLayer *devicecard_layer, DeviceType device_type) {
  devicecard_layer->device_type = device_type;
  layer_mark_dirty(devicecard_layer->layer);
}

// Sets device location
void devicecard_layer_set_location(DeviceCardLayer *devicecard_layer, const char *location) {
  strncpy(devicecard_layer->location, location, sizeof(devicecard_layer->location));
  devicecard_layer->location[sizeof(devicecard_layer->location)-1] = '\0';
  layer_mark_dirty(devicecard_layer->layer);
}

// Sets device name
void devicecard_layer_set_name(DeviceCardLayer *devicecard_layer, const char *name) {
  strncpy(devicecard_layer->name, name, sizeof(devicecard_layer->name));
  devicecard_layer->name[sizeof(devicecard_layer->name)-1] = '\0';
  layer_mark_dirty(devicecard_layer->layer);
}

// Sets device status
void devicecard_layer_set_status(DeviceCardLayer *devicecard_layer, DeviceStatus status) {
  devicecard_layer->status = status;
  
  switch (devicecard_layer->device_type) {
    case DTGarageDoor:
      // Initialize icon animation if garage door is opening/closing
      switch (status) {
        case DSOpening:
          devicecard_layer->animation_step = 5;
          if (devicecard_layer->animation_timer == NULL)
            devicecard_layer->animation_timer = app_timer_register(500, animate_icon, devicecard_layer);
          else
            app_timer_reschedule(devicecard_layer->animation_timer, 500);
          break;
        case DSClosing:
          devicecard_layer->animation_step = 0;
          if (devicecard_layer->animation_timer == NULL)
            devicecard_layer->animation_timer = app_timer_register(500, animate_icon, devicecard_layer);
          else
            app_timer_reschedule(devicecard_layer->animation_timer, 500);
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
  
  layer_mark_dirty(devicecard_layer->layer);
}

// Sets device status changed description
void devicecard_layer_set_status_changed(DeviceCardLayer *devicecard_layer, const char *status_changed) {
  strncpy(devicecard_layer->status_changed, status_changed, sizeof(devicecard_layer->status_changed));
  devicecard_layer->status_changed[sizeof(devicecard_layer->status_changed)-1] = '\0';
  layer_mark_dirty(devicecard_layer->layer);
}
