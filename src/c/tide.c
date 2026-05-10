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
  { // 0 — Monterey, CA — NOAA 9413450
    .name   = "Monterey",
    .msl_m  = 0.841f,
    .mhhw_m = 1.826f,
    .amp    = { 0.491f, 0.366f, 0.229f, 0.115f, 0.112f, 0.062f, 0.041f, 0.037f },
    .phi    = { 143.293f, 151.074f, 110.526f, 133.295f,
                251.646f, 159.038f, 201.279f,  30.747f }
  },
  { // 1 — San Diego, CA — NOAA 9410170
    .name   = "San Diego",
    .msl_m  = 0.897f,
    .mhhw_m = 1.745f,
    .amp    = { 0.542f, 0.337f, 0.214f, 0.106f, 0.126f, 0.076f, 0.039f, 0.066f },
    .phi    = { 181.293f, 162.274f, 121.426f, 143.195f,
                283.546f, 177.038f, 210.679f,  66.847f }
  },
  { // 2 — Miami, FL — NOAA 8723214 Virginia Key
    .name   = "Miami",
    .msl_m  = 0.342f,
    .mhhw_m = 0.683f,
    .amp    = { 0.298f, 0.032f, 0.028f, 0.009f, 0.065f, 0.092f, 0.006f, 0.014f },
    .phi    = { 285.293f, 108.174f,  24.026f,  85.495f,
                 25.446f, 163.438f, 105.279f, 133.147f }
  },
  { // 3 — New York Battery, NY — NOAA 8518750
    .name   = "New York",
    .msl_m  = 0.783f,
    .mhhw_m = 1.541f,
    .amp    = { 0.671f, 0.103f, 0.051f, 0.033f, 0.156f, 0.094f, 0.011f, 0.036f },
    .phi    = { 306.393f, 192.174f, 137.726f, 170.295f,
                 46.146f, 212.938f, 199.479f, 156.847f }
  },
  { // 4 — Honolulu, HI — NOAA 1612340
    .name   = "Honolulu",
    .msl_m  = 0.349f,
    .mhhw_m = 0.627f,
    .amp    = { 0.171f, 0.149f, 0.081f, 0.045f, 0.033f, 0.048f, 0.014f, 0.016f },
    .phi    = { 265.193f, 143.874f,  98.126f, 124.195f,
                357.846f, 179.838f, 186.479f, 155.647f }
  },
  { // 5 — Rio de Janeiro — IOC/DHN
    .name   = "Rio",
    .msl_m  = 0.550f,
    .mhhw_m = 0.900f,
    .amp    = { 0.390f, 0.074f, 0.060f, 0.024f, 0.066f, 0.055f, 0.011f, 0.020f },
    .phi    = { 159.593f, 185.674f, 125.026f, 164.095f,
                238.646f, 197.538f, 207.079f,  36.347f }
  },
  { // 6 — London (Sheerness), UK — UKHO/NOC
    .name   = "London",
    .msl_m  = 2.900f,
    .mhhw_m = 4.450f,
    .amp    = { 2.030f, 0.072f, 0.079f, 0.026f, 0.420f, 0.095f, 0.014f, 0.062f },
    .phi    = { 335.593f, 139.674f,  71.026f, 118.095f,
                 50.646f, 337.538f, 153.079f, 212.347f }
  },
  { // 7 — Singapore — MPA/IOC
    .name   = "Singapore",
    .msl_m  = 1.440f,
    .mhhw_m = 2.840f,
    .amp    = { 0.835f, 0.306f, 0.266f, 0.097f, 0.152f, 0.125f, 0.050f, 0.160f },
    .phi    = { 240.593f,  25.674f,  59.026f,   4.095f,
                319.646f, 112.538f, 155.079f, 117.347f }
  },
  { // 8 — Jeju, Korea — KHOA
    .name   = "Jeju",
    .msl_m  = 1.130f,
    .mhhw_m = 1.840f,
    .amp    = { 0.470f, 0.200f, 0.155f, 0.062f, 0.098f, 0.090f, 0.025f, 0.125f },
    .phi    = {  90.593f,  57.674f,  54.026f,  36.095f,
                185.646f, 112.538f, 132.079f, 327.347f }
  },
  { // 9 — Sydney (Fort Denison), AU — NTC/BOM
    .name   = "Sydney",
    .msl_m  = 0.550f,
    .mhhw_m = 0.950f,
    .amp    = { 0.500f, 0.115f, 0.100f, 0.040f, 0.101f, 0.050f, 0.018f, 0.136f },
    .phi    = {  63.593f, 337.674f, 335.026f, 316.095f,
                147.646f, 147.538f,  69.079f, 300.347f }
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
