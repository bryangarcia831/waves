#include "moon.h"

MoonPhase moon_calculate_phase(time_t now) {
  MoonPhase result;
  double days  = (double)(now - KNOWN_NEW_MOON_TS) / 86400.0;
  double phase = days / LUNAR_CYCLE_DAYS;
  phase        = phase - (long)phase;
  if (phase < 0) phase += 1.0;
  // cos(2π * phase) using Pebble's cos_lookup
  double c = (double)cos_lookup((int32_t)(phase * TRIG_MAX_ANGLE)) / TRIG_MAX_RATIO;
  result.illumination = (float)((1.0 - c) / 2.0);
  result.phase_day = (int)(phase * LUNAR_CYCLE_DAYS + 0.5);
  result.waxing = phase < 0.5;
  return result;
}

void moon_draw(GContext *ctx, GRect frame, MoonPhase phase, bool dark_mode) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "moon_draw start");
  GPoint center = GPoint(
    frame.origin.x + frame.size.w / 2,
    frame.origin.y + frame.size.h / 2
  );
  int r = (frame.size.h < frame.size.w ? frame.size.h : frame.size.w) / 2 - 2;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "r=%d", r);

  // 1. Dark base
  graphics_context_set_fill_color(ctx, dark_mode ? GColorDarkGray : GColorMidnightGreen);
  graphics_fill_circle(ctx, center, r);

  // 2. Lit portion — horizontal scanlines with elliptical terminator
  graphics_context_set_fill_color(ctx, GColorPictonBlue);

  // Terminator ellipse rx: 0 at half, ±r at new/full
  // k = 1 - 2*illumination maps illumination to [-1,1]
  float k = 1.0f - 2.0f * phase.illumination;
  // rx of the terminator ellipse (signed: positive = right of center)
  float term_rx = k * (float)r;

  for (int dy = -r; dy <= r; dy++) {
    // Circle edge x-extent at this row
    int s = r * r - dy * dy, edge = 0;
    while ((edge + 1) * (edge + 1) <= s) edge++;

    // Terminator x at this row (ellipse: x = term_rx * sqrt(1 - (dy/r)^2))
    float frac = 1.0f - (float)(dy * dy) / (float)(r * r);
    if (frac < 0.0f) frac = 0.0f;
    // Integer sqrt of frac*65536 to avoid float sqrt
    int frac_fixed = (int)(frac * 65536.0f);
    int sqrt_fixed = 0;
    while ((sqrt_fixed + 1) * (sqrt_fixed + 1) <= frac_fixed) sqrt_fixed++;
    int term_x = (int)(term_rx * (float)sqrt_fixed / 256.0f);

    int x_left, x_right;
    if (phase.waxing) {
      x_left  = center.x + term_x;
      x_right = center.x + edge;
    } else {
      x_left  = center.x - edge;
      x_right = center.x + term_x;
    }

    if (x_left > x_right) continue;
    int y = center.y + dy;
    graphics_fill_rect(ctx, GRect(x_left, y, x_right - x_left + 1, 1), 0, GCornerNone);
  }

  // 3. Longitude slice lines (orange, curved like meridians)
  graphics_context_set_stroke_color(ctx, dark_mode ? GColorDarkGray : GColorMediumAquamarine);
  graphics_context_set_stroke_width(ctx, 1);
  int rx_vals[] = { r/4, r/2, (r*3)/4 };
  for (int e = 0; e < 3; e++) {
    int rx = rx_vals[e];
    GPoint prev = GPoint(center.x, center.y - r);
    for (int seg = 1; seg <= 20; seg++) {
      int32_t angle = DEG_TO_TRIGANGLE(-90 + seg * 9);
      int x = center.x + rx * cos_lookup(angle) / TRIG_MAX_RATIO;
      int y = center.y +  r * sin_lookup(angle) / TRIG_MAX_RATIO;
      if ((prev.x-center.x)*(prev.x-center.x)+(prev.y-center.y)*(prev.y-center.y) <= r*r &&
          (x-center.x)*(x-center.x)+(y-center.y)*(y-center.y) <= r*r)
        graphics_draw_line(ctx, prev, GPoint(x, y));
      prev = GPoint(x, y);
    }
    // Mirror on left side
    prev = GPoint(center.x, center.y - r);
    for (int seg = 1; seg <= 20; seg++) {
      int32_t angle = DEG_TO_TRIGANGLE(-90 + seg * 9);
      int mx = center.x - rx * cos_lookup(angle) / TRIG_MAX_RATIO;
      int y  = center.y +  r * sin_lookup(angle) / TRIG_MAX_RATIO;
      GPoint lp = GPoint(center.x - (prev.x - center.x), prev.y);
      if ((lp.x-center.x)*(lp.x-center.x)+(lp.y-center.y)*(lp.y-center.y) <= r*r &&
          (mx-center.x)*(mx-center.x)+(y-center.y)*(y-center.y) <= r*r)
        graphics_draw_line(ctx, lp, GPoint(mx, y));
      prev = GPoint(center.x + rx * cos_lookup(angle) / TRIG_MAX_RATIO, y);
    }
  }
  // Center vertical longitude line
  graphics_draw_line(ctx, GPoint(center.x, center.y - r), GPoint(center.x, center.y + r));

  // 4. Outer ring (inner edge + outer edge with gap)
  graphics_context_set_stroke_color(ctx, dark_mode ? GColorDarkGray : GColorMediumAquamarine);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, center, r);

  // 5. Surrounding bezel ring
  graphics_context_set_stroke_color(ctx, GColorChromeYellow);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, center, r + 1);
}
