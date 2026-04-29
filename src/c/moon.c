#include "moon.h"
#include <math.h>

MoonPhase moon_calculate_phase(time_t now) {
  double days  = (double)(now - KNOWN_NEW_MOON_TS) / 86400.0;
  double phase = days / LUNAR_CYCLE_DAYS;
  phase        = phase - (long)phase;
  if (phase < 0) phase += 1.0;
  float illum  = (float)((1.0 - cos(2.0 * M_PI * phase)) / 2.0);
  return (MoonPhase){
    .illumination = illum,
    .phase_day    = (int)(phase * LUNAR_CYCLE_DAYS),
    .waxing       = phase < 0.5,
  };
}

void moon_draw(GContext *ctx, GRect frame, MoonPhase phase) {
  GPoint center = GPoint(
    frame.origin.x + frame.size.w / 2,
    frame.origin.y + frame.size.h / 2
  );
  int r = (frame.size.h < frame.size.w ? frame.size.h : frame.size.w) / 2 - 2;

  // 1. Dark base
  graphics_context_set_fill_color(ctx, GColorMidnightGreen);
  graphics_fill_circle(ctx, center, r);

  // 2. Lit portion — vertical scanlines within circle
  graphics_context_set_stroke_color(ctx, GColorPictonBlue);
  graphics_context_set_stroke_width(ctx, 1);

  int terminator_x = phase.waxing
    ? center.x + (int)((float)r * (1.0f - 2.0f * phase.illumination))
    : center.x - (int)((float)r * (1.0f - 2.0f * phase.illumination));

  int x_start = phase.waxing ? terminator_x    : center.x - r;
  int x_end   = phase.waxing ? center.x + r    : terminator_x;

  for (int x = x_start; x <= x_end; x++) {
    int dx = x - center.x;
    if (dx * dx > r * r) continue;
    int dy = (int)sqrt((float)(r * r - dx * dx));
    graphics_draw_line(ctx, GPoint(x, center.y - dy), GPoint(x, center.y + dy));
  }

  // 3. Meridian lines (globe effect) — ellipses of varying rx
  graphics_context_set_stroke_color(ctx, GColorMidnightGreen);
  graphics_context_set_stroke_width(ctx, 1);
  int rx_vals[] = { r/4, r/2, (r*3)/4 };
  for (int e = 0; e < 3; e++) {
    int rx = rx_vals[e];
    GPoint prev = GPoint(center.x, center.y - r);
    for (int s = 1; s <= 20; s++) {
      int32_t angle = DEG_TO_TRIGANGLE(-90 + s * 9);
      int x = center.x + rx * cos_lookup(angle) / TRIG_MAX_RATIO;
      int y = center.y +  r * sin_lookup(angle) / TRIG_MAX_RATIO;
      int dx0 = prev.x - center.x, dy0 = prev.y - center.y;
      int dx1 = x - center.x,      dy1 = y - center.y;
      if (dx0*dx0+dy0*dy0 <= r*r && dx1*dx1+dy1*dy1 <= r*r)
        graphics_draw_line(ctx, prev, GPoint(x, y));
      // Mirror on left side
      int mx = center.x - rx * cos_lookup(angle) / TRIG_MAX_RATIO;
      GPoint lp = GPoint(center.x - (prev.x - center.x), prev.y);
      int ldx0 = lp.x-center.x, ldy0=lp.y-center.y;
      int ldx1 = mx-center.x,   ldy1=y-center.y;
      if (ldx0*ldx0+ldy0*ldy0 <= r*r && ldx1*ldx1+ldy1*ldy1 <= r*r)
        graphics_draw_line(ctx, lp, GPoint(mx, y));
      prev = GPoint(x, y);
    }
  }

  // 4. Center vertical line
  graphics_draw_line(ctx, GPoint(center.x, center.y - r), GPoint(center.x, center.y + r));

  // 5. Blue outer ring
  graphics_context_set_stroke_color(ctx, GColorCobaltBlue);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, center, r);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, center, r + 2);
}
