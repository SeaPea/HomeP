#include "msg.h"
#include "common.h"
#include <pebble.h>

// Simple message window that can be set not to close on the back button (modal = true)
  
static bool s_modal = false;
static char s_msg[100];
static AppTimer *s_autohide = NULL;
  
// BEGIN AUTO-GENERATED UI CODE; DO NOT MODIFY
static Window *s_window;
static GFont s_res_gothic_24_bold;
static TextLayer *msg_layer;

static void initialise_ui(void) {
  s_window = window_create();
  window_set_background_color(s_window, COLOR_FALLBACK(GColorBulgarianRose, GColorBlack)); 
  IF_2(window_set_fullscreen(s_window, true));
  
  s_res_gothic_24_bold = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  // msg_layer
  msg_layer = text_layer_create(GRect(6, 21, 133, 125));
  text_layer_set_text(msg_layer, "Sent to Phone. Go to the Draw app Settings in the Pebble phone app");
  text_layer_set_text_alignment(msg_layer, GTextAlignmentCenter);
  text_layer_set_font(msg_layer, s_res_gothic_24_bold);
  text_layer_set_text_color(msg_layer, GColorWhite);
  text_layer_set_background_color(msg_layer, GColorClear);
  layer_add_child(window_get_root_layer(s_window), (Layer *)msg_layer);
}

static void destroy_ui(void) {
  window_destroy(s_window);
  text_layer_destroy(msg_layer);
}
// END AUTO-GENERATED UI CODE

static void handle_window_unload(Window* window) {
  destroy_ui();
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!s_modal) hide_msg();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
}

static void init_click_events(ClickConfigProvider click_config_provider) {
  window_set_click_config_provider(s_window, click_config_provider);
}

static void auto_hide(void *data) {
  s_autohide = NULL;
  hide_msg();
}

// Show or update window with the given message, modality, and seconds to hide the message after (0 means no auto-hide)
void show_msg(char *msg, bool modal, int hide_after) {
  s_modal = modal;
  
  if (s_window == NULL || !window_stack_contains_window(s_window)) {
    initialise_ui();
    init_click_events(click_config_provider);
    window_set_window_handlers(s_window, (WindowHandlers) {
      .unload = handle_window_unload,
    });
    window_stack_push(s_window, true);
  }
  
  strncpy(s_msg, msg, sizeof(s_msg));
  s_msg[sizeof(s_msg)-1] = '\0';
  text_layer_set_text(msg_layer, s_msg);
  
  // Set auto-hide timer
  if (hide_after == 0) {
    if (s_autohide != NULL) app_timer_cancel(s_autohide);
  } else {
    if (s_autohide != NULL)
      app_timer_reschedule(s_autohide, hide_after * 1000);
    else
      app_timer_register(hide_after * 1000, auto_hide, NULL);
  }
}

void hide_msg(void) {
  if (s_window != NULL && window_stack_contains_window(s_window)) window_stack_remove(s_window, true);
}

