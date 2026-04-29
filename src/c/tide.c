#include "tide.h"

TideData tide_calculate(time_t now) {
  TideData d = { .current_idx = TIDE_SAMPLES / 2 };
  for (int i = 0; i < TIDE_SAMPLES; i++) d.heights[i] = 0.0f;
  return d;
}

void tide_draw(GContext *ctx, GRect frame, TideData data) {
  (void)ctx; (void)frame; (void)data;
}
