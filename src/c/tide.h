#pragma once
#include <pebble.h>

// Monterey, CA — NOAA station 9413450
// 8-constituent harmonic tide prediction
#define TIDE_EPOCH           1735689600L  // Jan 1 2025 00:00:00 UTC
#define TIDE_MSL_M           0.841f       // MSL above MLLW (meters)
#define TIDE_MHHW_M          1.826f       // MHHW above MLLW (meters)
#define TIDE_N_CONSTITUENTS  8

#define TIDE_SAMPLES 48

typedef struct {
  float heights[TIDE_SAMPLES];  // normalized: -1.0=MLLW, +1.0=MHHW
  int   current_idx;            // index for "now" (TIDE_SAMPLES/2)
} TideData;

TideData tide_calculate(time_t now);
void     tide_draw(GContext *ctx, GRect frame, TideData data, bool dark_mode);
