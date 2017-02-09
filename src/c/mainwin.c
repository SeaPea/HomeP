#include "mainwin.h"
#include "devicecard_layer.h"
#include "common.h"
#include <pebble.h>

// Main application window for showing device list and changing device status
// (Device details are rendered using devicecard_layer)
  
// Define spot size and spacing (spots indicate which device is being viewed)
#define SPOT_RADIUS 3
#define SPOT_SPACING 4

#ifdef PBL_ROUND
#define DEV_LAYER_HEIGHT 148
#define DEV_LAYER_WIDTH 148
#else
#define DEV_LAYER_HEIGHT 136
#define DEV_LAYER_WIDTH 100
#endif

static DeviceCardLayer *s_devicecard_layer_old;
static GRect s_rect_above;
static GRect s_rect_onscreen;
static GRect s_rect_below;
static PropertyAnimation *s_pa_old;
static PropertyAnimation *s_pa_new;
static UIDeviceSwitchedCallback s_deviceswitched_callback;
static UIStatusChangeCallback s_statuschange_callback;

static Window *s_window;
static GBitmap *s_res_image_action_up;
static GBitmap *s_res_image_action_set;
static GBitmap *s_res_image_action_down;
static ActionBarLayer *s_actionbar_main;
static Layer *s_layer_spots;
static DeviceCardLayer *s_devicecard_layer;

#ifndef PBL_SDK_2
static StatusBarLayer *s_status_bar;
#endif

static void initialise_ui(void) {
  
  s_window = window_create();
  Layer *root_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(root_layer); 
  window_set_background_color(s_window, COLOR_FALLBACK(GColorBulgarianRose, GColorBlack)); 
  IF_2(window_set_fullscreen(s_window, false));
  
#ifdef PBL_ROUND
  int dev_layer_left = (bounds.size.w - DEV_LAYER_WIDTH)/2;
  int dev_layer_top = (bounds.size.h - DEV_LAYER_HEIGHT)/2;
#else
  int dev_layer_left = ((bounds.size.w - DEV_LAYER_WIDTH - ACTION_BAR_WIDTH)/2) + 4;
  int dev_layer_top = ((bounds.size.h - DEV_LAYER_HEIGHT - 14)/2) + IF_32(14, 0);
#endif
  
  s_rect_above = GRect(dev_layer_left, -(DEV_LAYER_HEIGHT+2), DEV_LAYER_WIDTH, DEV_LAYER_HEIGHT);
  s_rect_onscreen = GRect(dev_layer_left, dev_layer_top, DEV_LAYER_WIDTH, DEV_LAYER_HEIGHT);
  s_rect_below = GRect(dev_layer_left, bounds.size.h+2, DEV_LAYER_WIDTH, DEV_LAYER_HEIGHT);
  
  // s_devicecard_layer
  s_devicecard_layer = devicecard_layer_create(s_rect_onscreen);
  layer_add_child(root_layer, s_devicecard_layer->layer);
  
  // s_layer_spots
  s_layer_spots = layer_create(PBL_IF_RECT_ELSE(GRect((dev_layer_left/2)-SPOT_RADIUS, dev_layer_top, 
                                                (SPOT_RADIUS*2)+1 , DEV_LAYER_HEIGHT), bounds));
  layer_add_child(root_layer, (Layer *)s_layer_spots);
  
#ifndef PBL_SDK_2
  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, COLOR_FALLBACK(GColorBulgarianRose, GColorBlack), GColorWhite);
  layer_add_child(root_layer, status_bar_layer_get_layer(s_status_bar));
#endif
  
  s_res_image_action_up = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_UP);
  s_res_image_action_set = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_SET);
  s_res_image_action_down = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_DOWN);
  
  // s_actionbar_main
  s_actionbar_main = action_bar_layer_create();
  action_bar_layer_add_to_window(s_actionbar_main, s_window);
  action_bar_layer_set_background_color(s_actionbar_main, GColorWhite);
  action_bar_layer_set_icon(s_actionbar_main, BUTTON_ID_UP, s_res_image_action_up);
  action_bar_layer_set_icon(s_actionbar_main, BUTTON_ID_SELECT, s_res_image_action_set);
  action_bar_layer_set_icon(s_actionbar_main, BUTTON_ID_DOWN, s_res_image_action_down);
#ifdef PBL_RECT
  layer_set_frame(action_bar_layer_get_layer(s_actionbar_main), GRect(bounds.size.w-20, 0, 20, bounds.size.h));
  IF_3(layer_set_bounds(action_bar_layer_get_layer(s_actionbar_main), GRect(-5, 0, 30, bounds.size.h)));
#endif
  layer_add_child(root_layer, (Layer *)s_actionbar_main);
}

static void destroy_ui(void) {
  IF_3(status_bar_layer_destroy(s_status_bar));
  action_bar_layer_destroy(s_actionbar_main);
  devicecard_layer_destroy(s_devicecard_layer);
  devicecard_layer_destroy(s_devicecard_layer_old);
  layer_destroy(s_layer_spots);
  gbitmap_destroy(s_res_image_action_up);
  gbitmap_destroy(s_res_image_action_set);
  gbitmap_destroy(s_res_image_action_down);
  window_destroy(s_window);
}

// Draw the vertically stacked spots that indicate how many devices there are and which one is selected
static void spots_draw(Layer *layer, GContext *ctx) {
  GRect rect = layer_get_frame(layer);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  
  for (int y = 0; y < g_device_count; y++) {
#ifdef PBL_ROUND
    GPoint spot = gpoint_from_polar(GRect(rect.origin.x+((rect.size.w-DEV_LAYER_WIDTH)/4),
                                          rect.origin.y+((rect.size.h-DEV_LAYER_HEIGHT)/4),
                                          rect.size.w-((rect.size.w-DEV_LAYER_WIDTH)/2),
                                          rect.size.h-((rect.size.h-DEV_LAYER_HEIGHT)/2)), GOvalScaleModeFitCircle, 
                                   DEG_TO_TRIGANGLE(270+
                                      ((((SPOT_RADIUS*2)*g_device_count)+(SPOT_SPACING*(g_device_count-1)))/2) -
                                       (y*((SPOT_RADIUS*2)+SPOT_SPACING)+3)));
#else
    GPoint spot = GPoint((rect.size.w/2),
                                     (rect.size.h/2)-((((SPOT_RADIUS*2)*g_device_count)+(SPOT_SPACING*(g_device_count-1)))/2) +
                                     (y*((SPOT_RADIUS*2)+SPOT_SPACING)+3));
#endif
    if (g_device_selected == y)
      graphics_fill_circle(ctx, spot, SPOT_RADIUS);
    else
      graphics_draw_circle(ctx, spot, SPOT_RADIUS);
  }
}

// Event for when the animation of the old device being switched away from finishes
static void devicecard_anim_old_stopped(Animation *animation, bool finished, void *context) {
  if (s_devicecard_layer_old != NULL) {
    layer_remove_from_parent(s_devicecard_layer_old->layer);
    devicecard_layer_destroy(s_devicecard_layer_old);
    s_devicecard_layer_old = NULL;
  }
  property_animation_destroy((PropertyAnimation*)animation);
}

// Event for when the animation of the new device being switched in finishes
static void devicecard_anim_new_stopped(Animation *animation, bool finished, void *context) {
  property_animation_destroy((PropertyAnimation*)animation);
}

// Initialize the new device card properties
static void init_card() {
  
  devicecard_layer_set_type(s_devicecard_layer, DTUnknown);
  devicecard_layer_set_location(s_devicecard_layer, "");
  devicecard_layer_set_name(s_devicecard_layer, "");
  devicecard_layer_set_status(s_devicecard_layer, DSLoading);
  devicecard_layer_set_status_changed(s_devicecard_layer, "");
  
}

// Animate device cards to make it look like they are being scrolled
// up or down when switching devices
static void animate_cards(GRect *from, GRect *to) {
  
  s_pa_old = property_animation_create_layer_frame(s_devicecard_layer_old->layer, &s_rect_onscreen, to);
  s_pa_new = property_animation_create_layer_frame(s_devicecard_layer->layer, from, &s_rect_onscreen);
  
  animation_set_handlers(IF_32(property_animation_get_animation(s_pa_old), &(s_pa_old->animation)), 
                         (AnimationHandlers) {
    .stopped = devicecard_anim_old_stopped
  }, NULL);
  animation_set_handlers(IF_32(property_animation_get_animation(s_pa_new), &(s_pa_new->animation)), 
                         (AnimationHandlers) {
    .stopped = devicecard_anim_new_stopped
  }, NULL);
  
  animation_schedule((Animation*)s_pa_old);
  animation_schedule((Animation*)s_pa_new); 
  
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Select button triggers device status change
  if (s_statuschange_callback != NULL) s_statuschange_callback();
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Update selected device and scroll devices up 
  g_device_selected = (g_device_selected+(g_device_count-1)) % g_device_count;
  layer_mark_dirty(s_layer_spots);
  
  s_devicecard_layer_old = s_devicecard_layer;
  s_devicecard_layer = devicecard_layer_create(s_rect_above);
  Layer *root_layer = window_get_root_layer(s_window);
  layer_add_child(root_layer, s_devicecard_layer->layer);
  // Put statusbar and actionbar back on top
#ifndef PBL_SDK_2
  layer_remove_from_parent((Layer *)s_status_bar);
  layer_add_child(root_layer, (Layer *)s_status_bar);
#endif
  layer_remove_from_parent((Layer *)s_actionbar_main);
  layer_add_child(root_layer, (Layer *)s_actionbar_main);
  init_card();
  
  // Let main unit know device has been switched
  if (s_deviceswitched_callback != NULL) s_deviceswitched_callback();
  
  animate_cards(&s_rect_above, &s_rect_below);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Update selected device and scroll devices down
  g_device_selected = (g_device_selected+1) % g_device_count;
  layer_mark_dirty(s_layer_spots);
  
  s_devicecard_layer_old = s_devicecard_layer;
  s_devicecard_layer = devicecard_layer_create(s_rect_below);
  Layer *root_layer = window_get_root_layer(s_window);
  layer_add_child(root_layer, s_devicecard_layer->layer);
  // Put statusbar and actionbar back on top
#ifndef PBL_SDK_2
  layer_remove_from_parent((Layer *)s_status_bar);
  layer_add_child(root_layer, (Layer *)s_status_bar);
#endif
  layer_remove_from_parent((Layer *)s_actionbar_main);
  layer_add_child(root_layer, (Layer *)s_actionbar_main);
  init_card();
  
  // Let main unit know device has been switched
  if (s_deviceswitched_callback != NULL) s_deviceswitched_callback();
  
  animate_cards(&s_rect_below, &s_rect_above);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void handle_window_unload(Window* window) {
  destroy_ui();
}

// Update device spots, which indicates the device count
void show_device_count() {
  layer_mark_dirty(s_layer_spots);
}

void ui_register_deviceswitch(UIDeviceSwitchedCallback callback) {
  s_deviceswitched_callback = callback;
}

void ui_register_statuschange(UIStatusChangeCallback callback) {
  s_statuschange_callback = callback;
}

// Update the current device card with the given details
void show_device_details(const char *location, const char *name, const DeviceType device_type) {
  devicecard_layer_set_type(s_devicecard_layer, device_type);
  devicecard_layer_set_location(s_devicecard_layer, location);
  devicecard_layer_set_name(s_devicecard_layer, name);
}

// Update the current device card with the given status
void show_device_status(const DeviceStatus status, const char *status_changed) {
  devicecard_layer_set_status(s_devicecard_layer, status);
  devicecard_layer_set_status_changed(s_devicecard_layer, status_changed);
}

// Show the main window
void show_mainwin(void) {
  initialise_ui();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .unload = handle_window_unload,
  });
  
  layer_set_update_proc(s_layer_spots, spots_draw);
  action_bar_layer_set_click_config_provider(s_actionbar_main, click_config_provider);
  window_stack_push(s_window, true);
}

// Hide and free main window resources
void hide_mainwin(void) {
  window_stack_remove(s_window, true);
}

// Indicates if the main window is on the window stack
bool showing_mainwin(void) {
  if (s_window != NULL && window_stack_contains_window(s_window))
    return true;
  else
    return false;
}
