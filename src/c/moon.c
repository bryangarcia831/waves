#include "moon.h"

MoonPhase moon_calculate_phase(time_t now) {
  return (MoonPhase){ .illumination = 0.5f, .phase_day = 14, .waxing = true };
}

void moon_draw(GContext *ctx, GRect frame, MoonPhase phase) {
  (void)ctx; (void)frame; (void)phase;
}
