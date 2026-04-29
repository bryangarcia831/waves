#pragma once
#include <pebble.h>

#define TIDE_SAMPLES          17
#define TIDAL_PERIOD_SECONDS  (745 * 60)
#define KNOWN_NEW_MOON_TS     947182440

typedef struct {
  float heights[TIDE_SAMPLES];  // -1.0 (low) to +1.0 (high)
  int   current_idx;            // always TIDE_SAMPLES/2
} TideData;

TideData tide_calculate(time_t now);
void     tide_draw(GContext *ctx, GRect frame, TideData data);
