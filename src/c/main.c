#include <pebble.h>
#include <stdlib.h>
#include "moon.h"
#include "tide.h"

// ── Dimensions ───────────────────────────────────────────────────────────────
#define SCREEN_W      200
#define SCREEN_H      228
#define TOP_BAR_H      15
#define DAY_DATE_H     36
#define TIDE_MOON_H    60
#define TIME_H        104
#define BOTTOM_BAR_H   14
#define CHAMFER_SIZE   12

// ── Runtime colors (set by dark mode toggle) ──────────────────────────────────
static GColor g_lcd_bg;
static GColor g_navy;
static GColor g_digit;
static GColor g_ghost;
static GColor g_label;

static bool s_dark_mode = false;

static void apply_color_scheme(void) {
  if (s_dark_mode) {
    g_lcd_bg = GColorBlack;
    g_navy   = GColorOxfordBlue;
    g_digit  = GColorWhite;
    g_ghost  = GColorBlack;
    g_label  = GColorPictonBlue;
  } else {
    g_lcd_bg = GColorMediumAquamarine;
    g_navy   = GColorOxfordBlue;
    g_digit  = GColorBlack;
    g_ghost  = GColorCeleste;
    g_label  = GColorPictonBlue;
  }
}

// ── Persist keys ─────────────────────────────────────────────────────────────
#define KEY_USE_24H       0
#define KEY_DARK_MODE     1
#define KEY_DATE_FMT      2
#define KEY_TIDE_STATION  3

#define DATE_FMT_MMDD  0
#define DATE_FMT_DDMM  1

// ── State ────────────────────────────────────────────────────────────────────
static Window  *s_window;
static Layer   *s_window_layer;
static Layer   *s_top_bar_layer;
static Layer   *s_day_date_layer;
static Layer   *s_tide_moon_layer;
static Layer   *s_time_layer;
static Layer   *s_bottom_bar_layer;
static Layer   *s_chamfer_layer;

static bool     s_obstructed = false;

static GFont    s_font_dseg7_46;
static GFont    s_font_dseg14_24;
static GFont    s_font_illum_11;

static struct tm s_now;
static TideData  s_tide;
static MoonPhase s_moon;
static bool      s_use_24h       = false;
static uint8_t   s_date_fmt      = DATE_FMT_MMDD;
static int       s_tide_station  = 0;

static const char *DAY_NAMES[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};

// ── Unobstructed Area Handlers ─────────────────────────────────────────────────
static void unobstructed_will_change(GRect final_unobstructed, void *ctx) {
  GRect full_bounds = layer_get_bounds(s_window_layer);
  if (!grect_equal(&full_bounds, &final_unobstructed)) {
    layer_set_hidden(s_tide_moon_layer, true);
  } else if (s_obstructed) {
    layer_set_hidden(s_tide_moon_layer, false);
  }
}

static void unobstructed_change(AnimationProgress progress, void *ctx) {
  GRect bounds = layer_get_unobstructed_bounds(s_window_layer);
  GRect full = layer_get_bounds(s_window_layer);

  uint16_t percent = (progress * 100) / 65535;

  GRect time_frame = layer_get_frame(s_time_layer);
  GRect bb_frame = layer_get_frame(s_bottom_bar_layer);

  GRect normal_time = GRect(0, TOP_BAR_H + DAY_DATE_H + TIDE_MOON_H, SCREEN_W, TIME_H);
  GRect obstructed_time = GRect(0, TOP_BAR_H + DAY_DATE_H, SCREEN_W, bounds.size.h - TOP_BAR_H - DAY_DATE_H - BOTTOM_BAR_H);
  if (obstructed_time.size.h < 32) obstructed_time.size.h = 32;

  GRect normal_bb = GRect(0, SCREEN_H - BOTTOM_BAR_H, SCREEN_W, BOTTOM_BAR_H);
  GRect obstructed_bb = GRect(0, bounds.size.h - BOTTOM_BAR_H, SCREEN_W, BOTTOM_BAR_H);

  if (grect_equal(&bounds, &full)) {
    time_frame = normal_time;
    bb_frame = normal_bb;
  } else {
    int16_t time_y = normal_time.origin.y - (int16_t)((normal_time.origin.y - obstructed_time.origin.y) * percent / 100);
    int16_t time_h = normal_time.size.h - (int16_t)((normal_time.size.h - obstructed_time.size.h) * percent / 100);
    int16_t bb_y = normal_bb.origin.y - (int16_t)((normal_bb.origin.y - obstructed_bb.origin.y) * percent / 100);

    time_frame.origin.y = time_y;
    time_frame.size.h = time_h;
    bb_frame.origin.y = bb_y;
  }
  layer_set_frame(s_time_layer, time_frame);
  layer_set_frame(s_bottom_bar_layer, bb_frame);
}

static void unobstructed_did_change(void *ctx) {
  GRect full_bounds = layer_get_bounds(s_window_layer);
  GRect bounds = layer_get_unobstructed_bounds(s_window_layer);
  s_obstructed = !grect_equal(&full_bounds, &bounds);
  layer_set_hidden(s_tide_moon_layer, s_obstructed);

  GRect time_frame = layer_get_frame(s_time_layer);
  GRect bb_frame = layer_get_frame(s_bottom_bar_layer);
  if (!s_obstructed) {
    time_frame.origin.y = TOP_BAR_H + DAY_DATE_H + TIDE_MOON_H;
    time_frame.size.h = TIME_H;
    bb_frame.origin.y = SCREEN_H - BOTTOM_BAR_H;
    bb_frame.size.h = BOTTOM_BAR_H;
  } else {
    time_frame.origin.y = TOP_BAR_H + DAY_DATE_H;
    time_frame.size.h = bounds.size.h - TOP_BAR_H - DAY_DATE_H - BOTTOM_BAR_H;
    if (time_frame.size.h < 32) time_frame.size.h = 32;
    bb_frame.origin.y = bounds.size.h - BOTTOM_BAR_H;
    bb_frame.size.h = BOTTOM_BAR_H;
  }
  layer_set_frame(s_time_layer, time_frame);
  layer_set_frame(s_bottom_bar_layer, bb_frame);
}

// ── Forward declarations ──────────────────────────────────────────────────────
static void unobstructed_will_change(GRect final_unobstructed, void *ctx);
static void unobstructed_change(AnimationProgress progress, void *ctx);
static void unobstructed_did_change(void *ctx);

// ── Tick handler ─────────────────────────────────────────────────────────────
static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  s_now = *tick_time;
  layer_mark_dirty(s_time_layer);

  if (changed & DAY_UNIT)   layer_mark_dirty(s_day_date_layer);
  if (tick_time->tm_min % 15 == 0) {
    time_t now = time(NULL);
    s_tide = tide_calculate(now, s_tide_station);
    s_moon = moon_calculate_phase(now);
    layer_mark_dirty(s_tide_moon_layer);
  }
}

// ── Layer stubs (implemented in later tasks) ──────────────────────────────────
static void top_bar_update(Layer *layer, GContext *ctx) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "top_bar_update");
  GRect b = layer_get_bounds(layer);
  GFont sm = fonts_get_system_font(FONT_KEY_GOTHIC_09);

  graphics_context_set_fill_color(ctx, g_navy);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  graphics_context_set_text_color(ctx, g_label);
  int label_y = (b.size.h - 9) / 2;
  graphics_draw_text(ctx, "< Light", sm,
    GRect(14, label_y, 80, b.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, "Adjust >", sm,
    GRect(0, label_y, b.size.w - 6, b.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}
static void day_date_update(Layer *layer, GContext *ctx) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "day_date_update");
  GRect b = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, g_lcd_bg);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // 4px bottom divider — occupies last 4 rows, leaving 32px usable above
  graphics_context_set_fill_color(ctx, g_navy);
  graphics_fill_rect(ctx, GRect(0, b.size.h - 4, b.size.w, 4), 0, GCornerNone);

  // Content centered in usable 32px: DSEG14_24 is ~24px → y=4 centres it (4 top, 4 bottom)
  int content_h = b.size.h - 4;  // 32px
  int ty = (content_h - 24) / 2; // 4

  int margin = 10;
  graphics_context_set_text_color(ctx, g_ghost);
  graphics_draw_text(ctx, "888", s_font_dseg14_24,
    GRect(margin, ty, 96, content_h - ty), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, "8-88", s_font_dseg14_24,
    GRect(0, ty, b.size.w - 10, content_h - ty), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  graphics_context_set_text_color(ctx, g_digit);
  graphics_draw_text(ctx, DAY_NAMES[s_now.tm_wday], s_font_dseg14_24,
    GRect(margin, ty, 96, content_h - ty), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  char date_buf[16];
  if (s_date_fmt == DATE_FMT_DDMM) {
    snprintf(date_buf, sizeof(date_buf), "%02d-%02d", s_now.tm_mday, s_now.tm_mon+1);
  } else {
    snprintf(date_buf, sizeof(date_buf), "%02d-%02d", s_now.tm_mon+1, s_now.tm_mday);
  }
  graphics_draw_text(ctx, date_buf, s_font_dseg14_24,
    GRect(0, ty, b.size.w - 10, content_h - ty), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}
static void tide_moon_update(Layer *layer, GContext *ctx) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "tide_moon_update");
  GRect b = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, g_lcd_bg);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // 4px bottom divider — matches day_date divider weight
  graphics_context_set_fill_color(ctx, g_navy);
  graphics_fill_rect(ctx, GRect(0, b.size.h - 4, b.size.w, 4), 0, GCornerNone);

  // Usable height = b.size.h - 4 = 56px
  // Equal padding: left | tide | gap | moon | right, all gaps = 8px
  int gap    = 8;
  int moon_w = 56;
  int tide_w = b.size.w - moon_w - 3 * gap;  // 200 - 56 - 24 = 120
  int usable = b.size.h - 4;                  // 56px above divider

  tide_draw(ctx, GRect(gap, 6, tide_w, usable - 12), s_tide, s_dark_mode);
  moon_draw(ctx, GRect(gap + tide_w + gap, 4, moon_w, usable - 8), s_moon, s_dark_mode);
}
static void time_update(Layer *layer, GContext *ctx) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "time_update");
  GRect b = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, g_lcd_bg);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  char time_buf[8];
  if (s_use_24h) {
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", s_now.tm_hour, s_now.tm_min);
  } else {
    int hour12 = s_now.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", hour12, s_now.tm_min);
  }

  {
    int margin  = 10;
    int ap_w    = s_use_24h ? 0 : 22;
    int digit_w = b.size.w - ap_w - margin;

    int ty = (b.size.h - 48) / 2;
    if (ty < 4) ty = 4;

    graphics_context_set_text_color(ctx, g_ghost);
    graphics_draw_text(ctx, "88:88", s_font_dseg7_46,
      GRect(margin, ty, digit_w, 48),
      GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    graphics_context_set_text_color(ctx, g_digit);
    graphics_draw_text(ctx, time_buf, s_font_dseg7_46,
      GRect(margin, ty, digit_w, 48),
      GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    if (!s_use_24h) {
      bool is_pm  = s_now.tm_hour >= 12;
      int ap_x    = b.size.w - ap_w;
      int ap_top  = ty;
      int ap_h    = 18;
      int ap_spacing = 6;

      graphics_context_set_text_color(ctx, g_ghost);
      graphics_draw_text(ctx, "A", s_font_dseg14_24,
        GRect(ap_x, ap_top, ap_w, ap_h),
        GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
      graphics_draw_text(ctx, "P", s_font_dseg14_24,
        GRect(ap_x, ap_top + ap_h + ap_spacing, ap_w, ap_h),
        GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

      graphics_context_set_text_color(ctx, g_digit);
      graphics_draw_text(ctx, is_pm ? "P" : "A", s_font_dseg14_24,
        GRect(ap_x, is_pm ? ap_top + ap_h + ap_spacing : ap_top, ap_w, ap_h),
        GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    }
  }
}
static void bottom_bar_update(Layer *layer, GContext *ctx) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "bottom_bar_update");
  GRect b = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, g_navy);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  GFont sm = fonts_get_system_font(FONT_KEY_GOTHIC_09);

  graphics_context_set_text_color(ctx, g_label);
  int illum_y = (b.size.h - 11) / 2;
  int mode_y = (b.size.h - 9) / 2;
  graphics_draw_text(ctx, "Illuminator", s_font_illum_11,
    GRect(0, illum_y, b.size.w, b.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, "< Start", sm,
    GRect(14, mode_y, 80, b.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, "Mode >", sm,
    GRect(0, mode_y, b.size.w - 6, b.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}
static void chamfer_update(Layer *layer, GContext *ctx) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "chamfer_update");
  GRect b = layer_get_bounds(layer);
  int W = b.size.w, H = b.size.h, C = CHAMFER_SIZE;
  graphics_context_set_fill_color(ctx, GColorBlack);

  GPoint bl[3] = {GPoint(0,H-C), GPoint(C,H),   GPoint(0,H)};
  GPoint br[3] = {GPoint(W,H-C), GPoint(W-C,H), GPoint(W,H)};

  GPathInfo pi;
  GPath *p;

  pi = (GPathInfo){3, bl}; p = gpath_create(&pi); gpath_draw_filled(ctx, p); gpath_destroy(p);
  pi = (GPathInfo){3, br}; p = gpath_create(&pi); gpath_draw_filled(ctx, p); gpath_destroy(p);
}

// ── App lifecycle ─────────────────────────────────────────────────────────────
static void window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "window_load start");
  s_font_dseg7_46  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_48));
  APP_LOG(APP_LOG_LEVEL_DEBUG, "font 46 loaded: %p", s_font_dseg7_46);
  s_font_dseg14_24 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DATE_24));
  APP_LOG(APP_LOG_LEVEL_DEBUG, "font 24 loaded: %p", s_font_dseg14_24);
  s_font_illum_11  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ILLUM_11));
  APP_LOG(APP_LOG_LEVEL_DEBUG, "illum font loaded: %p", s_font_illum_11);

  apply_color_scheme();

  Layer *root = window_get_root_layer(window);
  s_window_layer = root;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "got root layer");
  int y = 0;

#define MAKE_LAYER(var, h, proc) \
  var = layer_create(GRect(0, y, SCREEN_W, h)); \
  APP_LOG(APP_LOG_LEVEL_DEBUG, "created " #var); \
  layer_set_update_proc(var, proc); \
  layer_add_child(root, var); \
  y += h;

  MAKE_LAYER(s_top_bar_layer,    TOP_BAR_H,    top_bar_update)
  MAKE_LAYER(s_day_date_layer,   DAY_DATE_H,   day_date_update)
  MAKE_LAYER(s_tide_moon_layer,  TIDE_MOON_H,  tide_moon_update)
  MAKE_LAYER(s_time_layer,       TIME_H,       time_update)
  MAKE_LAYER(s_bottom_bar_layer, BOTTOM_BAR_H, bottom_bar_update)
#undef MAKE_LAYER
  APP_LOG(APP_LOG_LEVEL_DEBUG, "all main layers created");

  s_chamfer_layer = layer_create(GRect(0, 0, SCREEN_W, SCREEN_H));
  layer_set_update_proc(s_chamfer_layer, chamfer_update);
  layer_add_child(root, s_chamfer_layer);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "chamfer layer done");

  UnobstructedAreaHandlers handlers = {
    .will_change = unobstructed_will_change,
    .change      = unobstructed_change,
    .did_change = unobstructed_did_change
  };
  unobstructed_area_service_subscribe(handlers, NULL);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "unobstructed area subscribed");

  time_t now = time(NULL);
  s_tide = tide_calculate(now, s_tide_station);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "tide calculated");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "about to call moon_calculate_phase");
  s_moon = moon_calculate_phase(now);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "moon calculated, phase_day=%d", s_moon.phase_day);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "tick subscribed, load done");
}

static void window_unload(Window *window) {
  layer_destroy(s_top_bar_layer);
  layer_destroy(s_day_date_layer);
  layer_destroy(s_tide_moon_layer);
  layer_destroy(s_time_layer);
  layer_destroy(s_bottom_bar_layer);
  layer_destroy(s_chamfer_layer);
  fonts_unload_custom_font(s_font_dseg7_46);
  fonts_unload_custom_font(s_font_dseg14_24);
  fonts_unload_custom_font(s_font_illum_11);
}

static void inbox_received(DictionaryIterator *iter, void *ctx) {
  bool dirty_time = false, dirty_all = false;

  Tuple *t = dict_find(iter, MESSAGE_KEY_USE_24H);
  if (t) {
    s_use_24h = t->value->int32 != 0;
    persist_write_bool(KEY_USE_24H, s_use_24h);
    dirty_time = true;
  }

  t = dict_find(iter, MESSAGE_KEY_DARK_MODE);
  if (t) {
    s_dark_mode = t->value->int32 != 0;
    persist_write_bool(KEY_DARK_MODE, s_dark_mode);
    apply_color_scheme();
    dirty_all = true;
  }

  t = dict_find(iter, MESSAGE_KEY_DATE_FMT);
  if (t) {
    s_date_fmt = (uint8_t)((t->type == TUPLE_CSTRING)
      ? atoi(t->value->cstring) : t->value->int32);
    if (s_date_fmt > DATE_FMT_DDMM) s_date_fmt = DATE_FMT_MMDD;
    persist_write_int(KEY_DATE_FMT, s_date_fmt);
    dirty_all = true;
  }

  t = dict_find(iter, MESSAGE_KEY_TIDE_STATION);
  if (t) {
    s_tide_station = (t->type == TUPLE_CSTRING)
      ? atoi(t->value->cstring) : t->value->int32;
    if (s_tide_station < 0 || s_tide_station >= TIDE_STATION_COUNT) s_tide_station = 0;
    persist_write_int(KEY_TIDE_STATION, s_tide_station);
    time_t now = time(NULL);
    s_tide = tide_calculate(now, s_tide_station);
    dirty_all = true;
  }

  if (dirty_all) {
    layer_mark_dirty(s_top_bar_layer);
    layer_mark_dirty(s_day_date_layer);
    layer_mark_dirty(s_tide_moon_layer);
    layer_mark_dirty(s_time_layer);
    layer_mark_dirty(s_bottom_bar_layer);
  } else if (dirty_time) {
    layer_mark_dirty(s_time_layer);
  }
}

static void init(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "init start");
  s_use_24h = persist_exists(KEY_USE_24H)
    ? persist_read_bool(KEY_USE_24H) : false;
  s_dark_mode = persist_exists(KEY_DARK_MODE)
    ? persist_read_bool(KEY_DARK_MODE) : false;
  s_date_fmt = persist_exists(KEY_DATE_FMT)
    ? persist_read_int(KEY_DATE_FMT) : DATE_FMT_MMDD;
  if (s_date_fmt > DATE_FMT_DDMM) s_date_fmt = DATE_FMT_MMDD;
  s_tide_station = persist_exists(KEY_TIDE_STATION)
    ? persist_read_int(KEY_TIDE_STATION) : 0;
  if (s_tide_station < 0 || s_tide_station >= TIDE_STATION_COUNT) s_tide_station = 0;

  time_t now = time(NULL);
  struct tm *t = localtime(&now); if (t) s_now = *t;

  app_message_register_inbox_received(inbox_received);
  app_message_open(256, 64);

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = window_load,
    .unload = window_unload,
  });
  APP_LOG(APP_LOG_LEVEL_DEBUG, "pushing window");
  window_stack_push(s_window, true);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "init done");
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
