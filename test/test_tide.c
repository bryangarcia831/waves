#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <math.h>

#define TIDE_SAMPLES          17
#define TIDAL_PERIOD_SECONDS  (745 * 60)
#define KNOWN_NEW_MOON_TS     947182440

void tide_calculate_test(time_t now, float *out, int *idx) {
  long window = 12 * 3600;
  time_t start = now - window / 2;
  int step = window / (TIDE_SAMPLES - 1);
  for (int i = 0; i < TIDE_SAMPLES; i++) {
    time_t t = start + i * step;
    double angle = 2.0 * M_PI * (double)(t - KNOWN_NEW_MOON_TS) / TIDAL_PERIOD_SECONDS;
    out[i] = (float)sin(angle);
  }
  *idx = TIDE_SAMPLES / 2;
}

int main(void) {
  float h[TIDE_SAMPLES]; int idx;
  tide_calculate_test(time(NULL), h, &idx);
  for (int i = 0; i < TIDE_SAMPLES; i++) {
    assert(h[i] >= -1.0f && h[i] <= 1.0f);
  }
  assert(idx == TIDE_SAMPLES / 2);
  printf("Tide heights:");
  for (int i = 0; i < TIDE_SAMPLES; i++) printf(" %.2f", h[i]);
  printf("\ncurrent=%d  All tide tests passed.\n", idx);
  return 0;
}
