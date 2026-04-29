#include "tide.h"
#include <math.h>

TideData tide_calculate(time_t now) {
  TideData d;
  d.current_idx = TIDE_SAMPLES / 2;
  long window   = 12 * 3600;
  time_t start  = now - window / 2;
  int step      = (int)(window / (TIDE_SAMPLES - 1));
  for (int i = 0; i < TIDE_SAMPLES; i++) {
    double angle = 2.0 * M_PI * (double)(start + i*step - KNOWN_NEW_MOON_TS)
                   / TIDAL_PERIOD_SECONDS;
    d.heights[i] = (float)sin(angle);
  }
  return d;
}

void tide_draw(GContext *ctx, GRect frame, TideData data) {
  int ox = frame.origin.x, oy = frame.origin.y;
  int w  = frame.size.w,   h  = frame.size.h;
  int pad = 4;

  // height val [-1,1] → pixel y (top = high tide, bottom = low tide)
  #define H2Y(v) ((int)(oy + pad + (1.0f - (v)) * 0.5f * (h - pad*2)))

  // Faint grid
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(ox, oy+pad),    GPoint(ox+w, oy+pad));
  graphics_draw_line(ctx, GPoint(ox, oy+h/2),    GPoint(ox+w, oy+h/2));
  graphics_draw_line(ctx, GPoint(ox, oy+h-pad),  GPoint(ox+w, oy+h-pad));

  // H / L labels
  GFont sys = fonts_get_system_font(FONT_KEY_FONT_FALLBACK);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, "H", sys, GRect(ox, oy,     8, 10),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, "L", sys, GRect(ox, oy+h-10, 8, 10),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

  // Wave line
  graphics_context_set_stroke_color(ctx, GColorCobaltBlue);
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 1; i < TIDE_SAMPLES; i++) {
    int x0 = ox + (i-1) * w / (TIDE_SAMPLES-1);
    int x1 = ox +  i    * w / (TIDE_SAMPLES-1);
    graphics_draw_line(ctx, GPoint(x0, H2Y(data.heights[i-1])),
                            GPoint(x1, H2Y(data.heights[i])));
  }

  // Orange dot at current position
  int cx = ox + data.current_idx * w / (TIDE_SAMPLES-1);
  int cy = H2Y(data.heights[data.current_idx]);
  graphics_context_set_fill_color(ctx, GColorChromeYellow);
  graphics_fill_circle(ctx, GPoint(cx, cy), 4);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, GPoint(cx, cy), 4);

  #undef H2Y
}
