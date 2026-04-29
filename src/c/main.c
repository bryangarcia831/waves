#include <pebble.h>
#include "moon.h"
#include "tide.h"

// ── Dimensions ───────────────────────────────────────────────────────────────
#define SCREEN_W      200
#define SCREEN_H      228
#define TOP_BAR_H      14
#define DAY_DATE_H     36
#define TIDE_MOON_H    60
#define TIME_H        104
#define BOTTOM_BAR_H   14
#define CHAMFER_SIZE   12

// ── Colors ───────────────────────────────────────────────────────────────────
#define COLOR_LCD_BG   GColorMediumAquamarine
#define COLOR_NAVY     GColorOxfordBlue
#define COLOR_DIGIT    GColorBlack
#define COLOR_GHOST    GColorCeleste
#define COLOR_LABEL    GColorPictonBlue

// ── Persist keys ─────────────────────────────────────────────────────────────
#define KEY_SHOW_SECONDS 0

// ── State ────────────────────────────────────────────────────────────────────
static Window  *s_window;
static Layer   *s_top_bar_layer;
static Layer   *s_day_date_layer;
static Layer   *s_tide_moon_layer;
static Layer   *s_time_layer;
static Layer   *s_bottom_bar_layer;
static Layer   *s_chamfer_layer;

static GFont    s_font_dseg7_46;
static GFont    s_font_dseg7_18;
static GFont    s_font_dseg14_24;

static struct tm s_now;
static TideData  s_tide;
static MoonPhase s_moon;
static bool      s_show_seconds = false;

static const char *DAY_NAMES[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};

// ── Forward declarations ──────────────────────────────────────────────────────
static void update_tick_subscription(void);

// ── Tick handler ─────────────────────────────────────────────────────────────
static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  s_now = *tick_time;
  layer_mark_dirty(s_time_layer);

  if (changed & DAY_UNIT)   layer_mark_dirty(s_day_date_layer);
  if (tick_time->tm_min % 15 == 0 || (changed & HOUR_UNIT)) {
    time_t now = time(NULL);
    s_tide = tide_calculate(now);
    s_moon = moon_calculate_phase(now);
    layer_mark_dirty(s_tide_moon_layer);
  }
}

// ── Layer stubs (implemented in later tasks) ──────────────────────────────────
static void top_bar_update(Layer *layer, GContext *ctx)    {}
static void day_date_update(Layer *layer, GContext *ctx)   {}
static void tide_moon_update(Layer *layer, GContext *ctx)  {}
static void time_update(Layer *layer, GContext *ctx)       {}
static void bottom_bar_update(Layer *layer, GContext *ctx) {}
static void chamfer_update(Layer *layer, GContext *ctx)    {}

// ── App lifecycle ─────────────────────────────────────────────────────────────
static void window_load(Window *window) {
  s_font_dseg7_46  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DSEG7_46));
  s_font_dseg7_18  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DSEG7_18));
  s_font_dseg14_24 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DSEG14_24));

  Layer *root = window_get_root_layer(window);
  int y = 0;

#define MAKE_LAYER(var, h, proc) \
  var = layer_create(GRect(0, y, SCREEN_W, h)); \
  layer_set_update_proc(var, proc); \
  layer_add_child(root, var); \
  y += h;

  MAKE_LAYER(s_top_bar_layer,    TOP_BAR_H,    top_bar_update)
  MAKE_LAYER(s_day_date_layer,   DAY_DATE_H,   day_date_update)
  MAKE_LAYER(s_tide_moon_layer,  TIDE_MOON_H,  tide_moon_update)
  MAKE_LAYER(s_time_layer,       TIME_H,       time_update)
  MAKE_LAYER(s_bottom_bar_layer, BOTTOM_BAR_H, bottom_bar_update)
#undef MAKE_LAYER

  s_chamfer_layer = layer_create(GRect(0, 0, SCREEN_W, SCREEN_H));
  layer_set_update_proc(s_chamfer_layer, chamfer_update);
  layer_add_child(root, s_chamfer_layer);

  time_t now = time(NULL);
  s_tide = tide_calculate(now);
  s_moon = moon_calculate_phase(now);
}

static void window_unload(Window *window) {
  layer_destroy(s_top_bar_layer);
  layer_destroy(s_day_date_layer);
  layer_destroy(s_tide_moon_layer);
  layer_destroy(s_time_layer);
  layer_destroy(s_bottom_bar_layer);
  layer_destroy(s_chamfer_layer);
  fonts_unload_custom_font(s_font_dseg7_46);
  fonts_unload_custom_font(s_font_dseg7_18);
  fonts_unload_custom_font(s_font_dseg14_24);
}

static void update_tick_subscription(void) {
  TimeUnits units = s_show_seconds ? (SECOND_UNIT | MINUTE_UNIT | DAY_UNIT)
                                   : (MINUTE_UNIT | DAY_UNIT);
  tick_timer_service_subscribe(units, tick_handler);
}

static void inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *t = dict_find(iter, MESSAGE_KEY_SHOW_SECONDS);
  if (t) {
    s_show_seconds = t->value->int32 != 0;
    persist_write_bool(KEY_SHOW_SECONDS, s_show_seconds);
    update_tick_subscription();
    layer_mark_dirty(s_time_layer);
  }
}

static void init(void) {
  s_show_seconds = persist_exists(KEY_SHOW_SECONDS)
    ? persist_read_bool(KEY_SHOW_SECONDS) : false;

  time_t now = time(NULL);
  localtime_r(&now, &s_now);

  app_message_open(64, 64);
  app_message_register_inbox_received(inbox_received);

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  update_tick_subscription();
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
