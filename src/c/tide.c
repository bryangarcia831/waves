#include "tide.h"

// ── Monterey CA (NOAA 9413450) harmonic constituents ─────────────────────────
// Order: M2, K1, O1, P1, N2, SA, Q1, K2
// Amplitudes in meters (from NOAA 5-year analysis 2012-2016)
static const float TIDE_AMP[TIDE_N_CONSTITUENTS] = {
  0.491f, 0.366f, 0.229f, 0.115f, 0.112f, 0.062f, 0.041f, 0.037f
};
// Speeds in degrees/hour
static const float TIDE_SPEED[TIDE_N_CONSTITUENTS] = {
  28.984104f, 15.041069f, 13.943035f, 14.958931f,
  28.439730f,  0.041069f, 13.398661f, 30.082138f
};
// phi0 = V0(t0) + u(t0) - kappa_GMT  [degrees], t0 = Jan 1 2025 00:00 UTC
// Optimised via least-squares fit to NOAA predictions for Monterey 9413450
// RMSE ~1.7 cm against Apr 29 2026 data
static const float TIDE_PHI[TIDE_N_CONSTITUENTS] = {
  133.639f, 162.743f,  56.653f, 218.858f,
  264.028f,  15.917f, 123.229f,  12.681f
};

// ── Height prediction ─────────────────────────────────────────────────────────
static float tide_height_m(time_t t) {
  // Hours from reference epoch (can be negative for past times)
  long dt_sec = (long)(t - TIDE_EPOCH);
  float dt_hr = (float)dt_sec / 3600.0f;

  float h = TIDE_MSL_M;
  for (int i = 0; i < TIDE_N_CONSTITUENTS; i++) {
    float angle_deg = TIDE_SPEED[i] * dt_hr + TIDE_PHI[i];
    // Convert to Pebble trig units and reduce to valid range
    int32_t trig_angle = (int32_t)(angle_deg / 360.0f * (float)TRIG_MAX_ANGLE);
    trig_angle = trig_angle % TRIG_MAX_ANGLE;
    if (trig_angle < 0) trig_angle += TRIG_MAX_ANGLE;
    h += TIDE_AMP[i] * (float)cos_lookup(trig_angle) / (float)TRIG_MAX_RATIO;
  }
  return h;
}

// Normalize meters-above-MLLW to [-1, 1] (MLLW=-1, MHHW=+1)
static float tide_normalize(float h_m) {
  float mid = TIDE_MHHW_M * 0.5f;      // 0.913 m
  float v = (h_m - mid) / mid;
  if (v >  1.0f) v =  1.0f;
  if (v < -1.0f) v = -1.0f;
  return v;
}

// ── Public API ────────────────────────────────────────────────────────────────
TideData tide_calculate(time_t now) {
  TideData d;
  d.current_idx = TIDE_SAMPLES / 2;

  // 24-hour window — shows a full day centered on now
  long window = 24 * 3600;
  time_t start = now - window / 2;
  int step = (int)(window / (TIDE_SAMPLES - 1));

  for (int i = 0; i < TIDE_SAMPLES; i++) {
    d.heights[i] = tide_normalize(tide_height_m(start + (time_t)(i * step)));
  }
  return d;
}

void tide_draw(GContext *ctx, GRect frame, TideData data, bool dark_mode) {
  int ox = frame.origin.x, oy = frame.origin.y;
  int w  = frame.size.w,   h  = frame.size.h;
  int pad = 4;

  // Reserve left margin for Y-axis labels
  int label_w = 14;
  int graph_x = ox + label_w;
  int graph_w = w - label_w;

  // height val [-1,1] → pixel y (top = high tide, bottom = low tide)
  #define H2Y(v) ((int)(oy + pad + (1.0f - (v)) * 0.5f * (h - pad*2)))

  GColor label_col = dark_mode ? GColorLightGray : GColorDarkGray;

  // Y-axis labels: H / M / L (high, mid, low)
  graphics_context_set_text_color(ctx, label_col);
  GFont small = fonts_get_system_font(FONT_KEY_GOTHIC_09);
  // High tide label
  graphics_draw_text(ctx, "H", small,
    GRect(ox, oy + pad - 4, label_w, 10), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  // Mid tide label
  graphics_draw_text(ctx, "M", small,
    GRect(ox, oy + h/2 - 5, label_w, 10), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  // Low tide label
  graphics_draw_text(ctx, "L", small,
    GRect(ox, oy + h - pad - 6, label_w, 10), GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  // Faint grid lines at high/mid/low
  graphics_context_set_stroke_color(ctx, dark_mode ? GColorDarkGray : GColorCeleste);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(graph_x, oy+pad),  GPoint(ox+w, oy+pad));
  graphics_draw_line(ctx, GPoint(graph_x, oy+h/2),  GPoint(ox+w, oy+h/2));
  graphics_draw_line(ctx, GPoint(graph_x, oy+h-pad), GPoint(ox+w, oy+h-pad));

  // Wave line
  graphics_context_set_stroke_color(ctx, GColorCobaltBlue);
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 1; i < TIDE_SAMPLES; i++) {
    int x0 = graph_x + (i-1) * graph_w / (TIDE_SAMPLES-1);
    int x1 = graph_x +  i    * graph_w / (TIDE_SAMPLES-1);
    graphics_draw_line(ctx,
      GPoint(x0, H2Y(data.heights[i-1])),
      GPoint(x1, H2Y(data.heights[i])));
  }

  // Orange dot at current position
  int cx = graph_x + data.current_idx * graph_w / (TIDE_SAMPLES-1);
  int cy = H2Y(data.heights[data.current_idx]);
  graphics_context_set_fill_color(ctx, GColorChromeYellow);
  graphics_fill_circle(ctx, GPoint(cx, cy), 4);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, GPoint(cx, cy), 4);

  #undef H2Y
}
