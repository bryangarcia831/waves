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
// phi0 = V0(TIDE_EPOCH) + u(TIDE_EPOCH) - kappa  [degrees]
// Precomputed for Jan 1 2025 00:00:00 UTC using Schureman astronomical args
static const float TIDE_PHI[TIDE_N_CONSTITUENTS] = {
  143.293f, 151.104f, 110.471f, 313.295f,
  251.646f, 159.036f, 201.224f,  30.780f
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

  long window = 12 * 3600;  // 12-hour window centred on now
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

  // height val [-1,1] → pixel y (top = high tide, bottom = low tide)
  #define H2Y(v) ((int)(oy + pad + (1.0f - (v)) * 0.5f * (h - pad*2)))

  // Faint grid lines at high/mid/low
  graphics_context_set_stroke_color(ctx, dark_mode ? GColorDarkGray : GColorLightGray);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(ox, oy+pad),    GPoint(ox+w, oy+pad));
  graphics_draw_line(ctx, GPoint(ox, oy+h/2),    GPoint(ox+w, oy+h/2));
  graphics_draw_line(ctx, GPoint(ox, oy+h-pad),  GPoint(ox+w, oy+h-pad));

  // Wave line
  graphics_context_set_stroke_color(ctx, GColorCobaltBlue);
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 1; i < TIDE_SAMPLES; i++) {
    int x0 = ox + (i-1) * w / (TIDE_SAMPLES-1);
    int x1 = ox +  i    * w / (TIDE_SAMPLES-1);
    graphics_draw_line(ctx,
      GPoint(x0, H2Y(data.heights[i-1])),
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
