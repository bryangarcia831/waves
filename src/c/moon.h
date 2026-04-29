#pragma once
#include <pebble.h>

#define LUNAR_CYCLE_DAYS  29.53058867f
#define KNOWN_NEW_MOON_TS 947182440

typedef struct {
  float illumination;
  int   phase_day;
  bool  waxing;
} MoonPhase;

MoonPhase moon_calculate_phase(time_t now);
void      moon_draw(GContext *ctx, GRect frame, MoonPhase phase, bool dark_mode);
