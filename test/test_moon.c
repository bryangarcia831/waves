// Host-runnable — no pebble.h needed
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>

#define LUNAR_CYCLE_DAYS  29.53058867
#define KNOWN_NEW_MOON_TS 947182440   // 2000-01-06 18:14 UTC

typedef struct { float illumination; int phase_day; bool waxing; } MoonPhase;

MoonPhase moon_calculate_phase(time_t now) {
  double days  = (double)(now - KNOWN_NEW_MOON_TS) / 86400.0;
  double phase = days / LUNAR_CYCLE_DAYS;
  phase        = phase - (long)phase;
  if (phase < 0) phase += 1.0;
  float illum  = (float)((1.0 - cos(2.0 * M_PI * phase)) / 2.0);
  int   pday   = (int)(phase * LUNAR_CYCLE_DAYS);
  return (MoonPhase){ illum, pday, phase < 0.5 };
}

int main(void) {
  // Known new moon → illumination ~0
  MoonPhase nm = moon_calculate_phase(947182440);
  printf("New moon:  illum=%.3f day=%d waxing=%d\n", nm.illumination, nm.phase_day, nm.waxing);
  assert(nm.illumination < 0.05f);
  assert(nm.waxing == true);

  // ~14.77 days later → full moon → illumination ~1
  MoonPhase fm = moon_calculate_phase(947182440 + (long)(14.77 * 86400));
  printf("Full moon: illum=%.3f day=%d waxing=%d\n", fm.illumination, fm.phase_day, fm.waxing);
  assert(fm.illumination > 0.95f);

  // Today — values in valid range
  MoonPhase today = moon_calculate_phase(time(NULL));
  printf("Today:     illum=%.3f day=%d waxing=%d\n", today.illumination, today.phase_day, today.waxing);
  assert(today.illumination >= 0.0f && today.illumination <= 1.0f);
  assert(today.phase_day >= 0 && today.phase_day <= 29);

  printf("All moon tests passed.\n");
  return 0;
}
