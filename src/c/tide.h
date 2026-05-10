#pragma once
#include <pebble.h>

#define TIDE_EPOCH           1735689600L  // Jan 1 2025 00:00:00 UTC
#define TIDE_N_CONSTITUENTS  8
#define TIDE_STATION_COUNT   10

#define TIDE_SAMPLES 48

typedef struct {
  const char  *name;
  float        msl_m;
  float        mhhw_m;
  float        amp[TIDE_N_CONSTITUENTS];
  float        phi[TIDE_N_CONSTITUENTS];
} TideStation;

typedef struct {
  float heights[TIDE_SAMPLES];
  int   current_idx;
} TideData;

extern const TideStation TIDE_STATIONS[TIDE_STATION_COUNT];

TideData tide_calculate(time_t now, int station_idx);
void     tide_draw(GContext *ctx, GRect frame, TideData data, bool dark_mode);
