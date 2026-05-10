#include "tide.h"

// ── Constituent speeds in degrees/hour (universal — same for all stations) ────
// Order: M2, K1, O1, P1, N2, SA, Q1, K2
static const float TIDE_SPEED[TIDE_N_CONSTITUENTS] = {
  28.984104f, 15.041069f, 13.943035f, 14.958931f,
  28.439730f,  0.041069f, 13.398661f, 30.082138f
};

// ── Station data ──────────────────────────────────────────────────────────────
// phi0 = V0(t0) + u(t0) - kappa_GMT  [degrees], t0 = Jan 1 2025 00:00 UTC
const TideStation TIDE_STATIONS[TIDE_STATION_COUNT] = {
  { // 0 — Monterey, CA (122°W) — NOAA 9413450
    .name   = "Monterey",
    .msl_m  = 0.841f,
    .mhhw_m = 1.826f,
    .amp    = { 0.491f, 0.366f, 0.229f, 0.115f, 0.112f, 0.062f, 0.041f, 0.037f },
    .phi    = { 133.639f, 162.743f,  56.653f, 218.858f,
                264.028f,  15.917f, 123.229f,  12.681f }
  },
  { // 1 — San Diego, CA (117°W) — NOAA 9410170
    .name   = "San Diego",
    .msl_m  = 0.897f,
    .mhhw_m = 1.745f,
    .amp    = { 0.542f, 0.337f, 0.214f, 0.106f, 0.126f, 0.076f, 0.039f, 0.066f },
    .phi    = { 196.19f, 156.27f, 157.22f, 152.13f, 198.84f, 102.44f, 144.75f, 198.84f }
  },
  { // 2 — Miami, FL (80°W) — NOAA 8723214 Virginia Key
    .name   = "Miami",
    .msl_m  = 0.342f,
    .mhhw_m = 0.683f,
    .amp    = { 0.298f, 0.032f, 0.028f, 0.009f, 0.065f, 0.092f, 0.006f, 0.014f },
    .phi    = { 300.19f, 102.17f, 59.82f, 94.43f, 300.74f, 88.84f, 39.35f, 265.14f }
  },
  { // 3 — New York Battery, NY (74°W) — NOAA 8518750
    .name   = "New York",
    .msl_m  = 0.783f,
    .mhhw_m = 1.541f,
    .amp    = { 0.671f, 0.103f, 0.051f, 0.033f, 0.156f, 0.094f, 0.011f, 0.036f },
    .phi    = { 321.29f, 186.17f, 173.52f, 179.23f, 321.44f, 138.34f, 133.55f, 288.84f }
  },
  { // 4 — Rio de Janeiro, Brazil (43°W) — IOC/DHN
    .name   = "Rio",
    .msl_m  = 0.55f,
    .mhhw_m = 0.90f,
    .amp    = { 0.340f, 0.073f, 0.057f, 0.023f, 0.059f, 0.055f, 0.010f, 0.092f },
    .phi    = { 307.49f, 207.67f, 207.82f, 196.03f, 310.94f, 152.94f, 215.15f, 304.34f }
  },
  { // 5 — London (Sheerness), UK (0°) — UKHO Admiralty
    .name   = "London",
    .msl_m  = 2.90f,
    .mhhw_m = 4.45f,
    .amp    = { 2.030f, 0.072f, 0.079f, 0.026f, 0.420f, 0.095f, 0.014f, 0.590f },
    .phi    = { 350.49f, 133.67f, 17.82f, 131.03f, 358.94f, 32.94f, 25.15f, 343.34f }
  },
  { // 6 — Singapore (104°E) — MPA/IOC
    .name   = "Singapore",
    .msl_m  = 1.44f,
    .mhhw_m = 2.84f,
    .amp    = { 0.835f, 0.306f, 0.266f, 0.097f, 0.152f, 0.125f, 0.050f, 0.227f },
    .phi    = { 240.49f, 25.67f, 58.82f, 26.03f, 239.94f, 112.94f, 95.15f, 235.34f }
  },
  { // 7 — Jeju, Korea (127°E) — KHOA
    .name   = "Jeju",
    .msl_m  = 1.13f,
    .mhhw_m = 1.84f,
    .amp    = { 0.470f, 0.200f, 0.155f, 0.062f, 0.098f, 0.090f, 0.025f, 0.125f },
    .phi    = { 90.49f, 57.67f, 53.82f, 48.03f, 85.94f, 112.94f, 52.15f, 89.34f }
  },
  { // 8 — Sydney, AU (151°E) — NTC/IOC Fort Denison
    .name   = "Sydney",
    .msl_m  = 0.55f,
    .mhhw_m = 0.95f,
    .amp    = { 0.496f, 0.118f, 0.105f, 0.040f, 0.101f, 0.050f, 0.018f, 0.136f },
    .phi    = { 63.49f, 338.67f, 335.82f, 337.03f, 67.94f, 147.94f, 329.15f, 57.34f }
  },
  { // 9 — Honolulu, HI (157°W) — NOAA 8587690
    .name   = "Honolulu",
    .msl_m  = 0.349f,
    .mhhw_m = 0.627f,
    .amp    = { 0.180f, 0.141f, 0.099f, 0.042f, 0.039f, 0.068f, 0.018f, 0.048f },
    .phi    = { 322.57f, 34.03f, 9.47f, 25.33f, 322.12f, 267.94f, 336.35f, 318.57f }
  }
};

// ── Height prediction ─────────────────────────────────────────────────────────
static float tide_height_m(time_t t, const TideStation *st) {
  long dt_sec = (long)(t - TIDE_EPOCH);
  float dt_hr = (float)dt_sec / 3600.0f;

  float h = st->msl_m;
  for (int i = 0; i < TIDE_N_CONSTITUENTS; i++) {
    float angle_deg = TIDE_SPEED[i] * dt_hr + st->phi[i];
    int32_t trig_angle = (int32_t)(angle_deg / 360.0f * (float)TRIG_MAX_ANGLE);
    trig_angle = trig_angle % TRIG_MAX_ANGLE;
    if (trig_angle < 0) trig_angle += TRIG_MAX_ANGLE;
    h += st->amp[i] * (float)cos_lookup(trig_angle) / (float)TRIG_MAX_RATIO;
  }
  return h;
}

// ── Normalize to [-1, 1] (MLLW=-1, MHHW=+1) ──────────────────────────────────
static float tide_normalize(float h_m, const TideStation *st) {
  float mid = st->mhhw_m * 0.5f;
  float v = (h_m - mid) / mid;
  if (v >  1.0f) v =  1.0f;
  if (v < -1.0f) v = -1.0f;
  return v;
}

// ── Public API ────────────────────────────────────────────────────────────────
TideData tide_calculate(time_t now, int station_idx) {
  if (station_idx < 0 || station_idx >= TIDE_STATION_COUNT) station_idx = 0;
  const TideStation *st = &TIDE_STATIONS[station_idx];

  TideData d;
  d.current_idx = TIDE_SAMPLES / 2;

  long window = 24 * 3600;
  time_t start = now - window / 2;
  int step = (int)(window / (TIDE_SAMPLES - 1));

  for (int i = 0; i < TIDE_SAMPLES; i++) {
    d.heights[i] = tide_normalize(tide_height_m(start + (time_t)(i * step), st), st);
  }
  return d;
}

void tide_draw(GContext *ctx, GRect frame, TideData data, bool dark_mode) {
  int ox = frame.origin.x, oy = frame.origin.y;
  int w  = frame.size.w,   h  = frame.size.h;
  int pad = 4;

  int label_w = 14;
  int graph_x = ox + label_w;
  int graph_w = w - label_w;

  #define H2Y(v) ((int)(oy + pad + (1.0f - (v)) * 0.5f * (h - pad*2)))

  GColor label_col = dark_mode ? GColorLightGray : GColorDarkGray;

  graphics_context_set_text_color(ctx, label_col);
  GFont small = fonts_get_system_font(FONT_KEY_GOTHIC_09);
  graphics_draw_text(ctx, "H", small,
    GRect(ox, oy + pad - 4, label_w, 10), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, "M", small,
    GRect(ox, oy + h/2 - 5, label_w, 10), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, "L", small,
    GRect(ox, oy + h - pad - 6, label_w, 10), GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  graphics_context_set_stroke_color(ctx, dark_mode ? GColorDarkGray : GColorCeleste);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(graph_x, oy+pad),    GPoint(ox+w, oy+pad));
  graphics_draw_line(ctx, GPoint(graph_x, oy+h/2),    GPoint(ox+w, oy+h/2));
  graphics_draw_line(ctx, GPoint(graph_x, oy+h-pad),  GPoint(ox+w, oy+h-pad));

  graphics_context_set_stroke_color(ctx, GColorCobaltBlue);
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 1; i < TIDE_SAMPLES; i++) {
    int x0 = graph_x + (i-1) * graph_w / (TIDE_SAMPLES-1);
    int x1 = graph_x +  i    * graph_w / (TIDE_SAMPLES-1);
    graphics_draw_line(ctx,
      GPoint(x0, H2Y(data.heights[i-1])),
      GPoint(x1, H2Y(data.heights[i])));
  }

  int cx = graph_x + data.current_idx * graph_w / (TIDE_SAMPLES-1);
  int cy = H2Y(data.heights[data.current_idx]);
  graphics_context_set_fill_color(ctx, GColorChromeYellow);
  graphics_fill_circle(ctx, GPoint(cx, cy), 4);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, GPoint(cx, cy), 4);

  #undef H2Y
}
