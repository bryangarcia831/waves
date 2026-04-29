# Casio Waves Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Casio G-LIDE-inspired watchface for Pebble Time 2 (Emery) with tide graph, moon phase globe, 7-segment fonts, and a seconds config toggle.

**Architecture:** Single-window watchface with five stacked layers (top bar → day/date → tide+moon → time → bottom bar) plus a chamfer overlay layer on top. Tide and moon are pure C math — no network. Settings persisted via `persist_read/write_bool`, toggled via a Clay config page over AppMessage.

**Tech Stack:** Pebble SDK 4.x (Repebble), C99, DSEG7/DSEG14 TTF fonts (compiled to bitmap by SDK), pebble-clay for config UI, libm for sin/cos.

---

## File Map

| File | Responsibility |
|------|----------------|
| `package.json` | App manifest, resource declarations, Clay dep, messageKeys |
| `wscript` | Pebble waf build config (adds `-lm`) |
| `src/c/main.c` | Window, layers, tick handler, settings, chamfer |
| `src/c/moon.h` | MoonPhase type + function declarations |
| `src/c/moon.c` | Phase calculation + `moon_draw()` |
| `src/c/tide.h` | TideData type + function declarations |
| `src/c/tide.c` | Height calculation + `tide_draw()` |
| `src/pkjs/index.js` | Clay entry point |
| `src/pkjs/config.js` | Clay config (seconds toggle) |
| `resources/fonts/DSEG7Classic-Regular.ttf` | 7-segment font |
| `resources/fonts/DSEG14Classic-Regular.ttf` | 14-segment font (supports letters) |
| `test/test_moon.c` | Host-runnable math test |
| `test/test_tide.c` | Host-runnable math test |

---

## Layer Layout (200×228px)

```
y=0   ┌─────────────────────────┐ h=14  TOP BAR    (dark navy)
      ├─────────────────────────┤ h=36  DAY/DATE   (seafoam)
      ├─────────────────────────┤ h=60  TIDE+MOON  (seafoam)
      ├─────────────────────────┤ h=104 TIME        (seafoam)
y=214 ├─────────────────────────┤ h=14  BOTTOM BAR (dark navy)
y=228 └─────────────────────────┘
      [CHAMFER OVERLAY — full 200×228, transparent except corners]
```

---

## Color Reference (Pebble 64-color palette)

| Role | Pebble Constant |
|------|----------------|
| LCD background | `GColorMediumAquamarine` |
| Navy bars | `GColorOxfordBlue` |
| Dividers | `GColorOxfordBlue` |
| Digits | `GColorBlack` |
| Ghost segments | `GColorCeleste` |
| Wave / moon ring | `GColorCobaltBlue` |
| Orange dot | `GColorChromeYellow` |
| Moon lit | `GColorPictonBlue` |
| Moon dark | `GColorMidnightGreen` |
| Bar label text | `GColorPictonBlue` |

---

## Task 1: Project Scaffold

**Files:** `package.json`, `wscript`, `src/c/main.c`, `.gitignore`

- [ ] **Step 1: Create directories**

```bash
mkdir -p src/c src/pkjs resources/fonts test
```

- [ ] **Step 2: Create `package.json`**

```json
{
  "name": "casio-waves",
  "author": "casio-waves",
  "version": "1.0.0",
  "keywords": ["pebble-app"],
  "pebble": {
    "sdkVersion": "4",
    "displayName": "Casio Waves",
    "uuid": "REPLACE-WITH-UUIDGEN-OUTPUT",
    "targetPlatforms": ["emery"],
    "watchapp": { "watchface": true },
    "capabilities": [],
    "messageKeys": ["SHOW_SECONDS"],
    "dependencies": { "pebble-clay": "*" },
    "resources": {
      "media": [
        {
          "type": "font",
          "name": "FONT_DSEG7_46",
          "file": "fonts/DSEG7Classic-Regular.ttf",
          "characterRegex": "[0-9:AMP ]",
          "compatibility": "2.7"
        },
        {
          "type": "font",
          "name": "FONT_DSEG7_18",
          "file": "fonts/DSEG7Classic-Regular.ttf",
          "characterRegex": "[0-9:AMP ]",
          "compatibility": "2.7"
        },
        {
          "type": "font",
          "name": "FONT_DSEG14_24",
          "file": "fonts/DSEG14Classic-Regular.ttf",
          "characterRegex": "[A-Z0-9/ \\-]",
          "compatibility": "2.7"
        }
      ]
    }
  }
}
```

Run `uuidgen` and paste the result into the `uuid` field.

- [ ] **Step 3: Create `wscript`**

```python
import os.path
top = '.'
out = 'build'

def options(ctx):
    ctx.load('pebble_sdk')

def configure(ctx):
    ctx.load('pebble_sdk')

def build(ctx):
    ctx.load('pebble_sdk')
    build_worker = os.path.exists('worker_src')
    binaries = []
    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.set_env(ctx.all_envs[platform])
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pebble_build_group(
            build_worker=build_worker,
            sources=ctx.path.ant_glob('src/c/**/*.c'),
            libraries=['m'],
        )
        binaries.append({'platform': platform, 'app_elf': app_elf})
    ctx.set_group('bundle')
    ctx.pebble_bundle(binaries=binaries)
```

- [ ] **Step 4: Create `src/c/main.c`**

```c
#include <pebble.h>
#include "moon.h"
#include "tide.h"

// ── Dimensions ───────────────────────────────────────────────────────────────
#define SCREEN_W      200
#define SCREEN_H      228
#define TOP_BAR_H      14
#define DAY_DATE_H     36
#define TIDE_MOON_H    60
#define TIME_H        104
#define BOTTOM_BAR_H   14
#define CHAMFER_SIZE   12

// ── Colors ───────────────────────────────────────────────────────────────────
#define COLOR_LCD_BG   GColorMediumAquamarine
#define COLOR_NAVY     GColorOxfordBlue
#define COLOR_DIGIT    GColorBlack
#define COLOR_GHOST    GColorCeleste
#define COLOR_LABEL    GColorPictonBlue

// ── Persist keys ─────────────────────────────────────────────────────────────
#define KEY_SHOW_SECONDS 0

// ── State ────────────────────────────────────────────────────────────────────
static Window  *s_window;
static Layer   *s_top_bar_layer;
static Layer   *s_day_date_layer;
static Layer   *s_tide_moon_layer;
static Layer   *s_time_layer;
static Layer   *s_bottom_bar_layer;
static Layer   *s_chamfer_layer;

static GFont    s_font_dseg7_46;
static GFont    s_font_dseg7_18;
static GFont    s_font_dseg14_24;

static struct tm s_now;
static TideData  s_tide;
static MoonPhase s_moon;
static bool      s_show_seconds = false;

static const char *DAY_NAMES[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};

// ── Forward declarations ──────────────────────────────────────────────────────
static void update_tick_subscription(void);

// ── Tick handler ─────────────────────────────────────────────────────────────
static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  s_now = *tick_time;
  layer_mark_dirty(s_time_layer);

  if (changed & DAY_UNIT)   layer_mark_dirty(s_day_date_layer);
  if (tick_time->tm_min % 15 == 0 || (changed & HOUR_UNIT)) {
    time_t now = time(NULL);
    s_tide = tide_calculate(now);
    s_moon = moon_calculate_phase(now);
    layer_mark_dirty(s_tide_moon_layer);
  }
}

// ── Layer stubs (implemented in later tasks) ──────────────────────────────────
static void top_bar_update(Layer *layer, GContext *ctx)    {}
static void day_date_update(Layer *layer, GContext *ctx)   {}
static void tide_moon_update(Layer *layer, GContext *ctx)  {}
static void time_update(Layer *layer, GContext *ctx)       {}
static void bottom_bar_update(Layer *layer, GContext *ctx) {}
static void chamfer_update(Layer *layer, GContext *ctx)    {}

// ── App lifecycle ─────────────────────────────────────────────────────────────
static void window_load(Window *window) {
  s_font_dseg7_46  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DSEG7_46));
  s_font_dseg7_18  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DSEG7_18));
  s_font_dseg14_24 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DSEG14_24));

  Layer *root = window_get_root_layer(window);
  int y = 0;

#define MAKE_LAYER(var, h, proc) \
  var = layer_create(GRect(0, y, SCREEN_W, h)); \
  layer_set_update_proc(var, proc); \
  layer_add_child(root, var); \
  y += h;

  MAKE_LAYER(s_top_bar_layer,    TOP_BAR_H,    top_bar_update)
  MAKE_LAYER(s_day_date_layer,   DAY_DATE_H,   day_date_update)
  MAKE_LAYER(s_tide_moon_layer,  TIDE_MOON_H,  tide_moon_update)
  MAKE_LAYER(s_time_layer,       TIME_H,       time_update)
  MAKE_LAYER(s_bottom_bar_layer, BOTTOM_BAR_H, bottom_bar_update)
#undef MAKE_LAYER

  s_chamfer_layer = layer_create(GRect(0, 0, SCREEN_W, SCREEN_H));
  layer_set_update_proc(s_chamfer_layer, chamfer_update);
  layer_add_child(root, s_chamfer_layer);

  time_t now = time(NULL);
  s_tide = tide_calculate(now);
  s_moon = moon_calculate_phase(now);
}

static void window_unload(Window *window) {
  layer_destroy(s_top_bar_layer);
  layer_destroy(s_day_date_layer);
  layer_destroy(s_tide_moon_layer);
  layer_destroy(s_time_layer);
  layer_destroy(s_bottom_bar_layer);
  layer_destroy(s_chamfer_layer);
  fonts_unload_custom_font(s_font_dseg7_46);
  fonts_unload_custom_font(s_font_dseg7_18);
  fonts_unload_custom_font(s_font_dseg14_24);
}

static void update_tick_subscription(void) {
  TimeUnits units = s_show_seconds ? (SECOND_UNIT | MINUTE_UNIT | DAY_UNIT)
                                   : (MINUTE_UNIT | DAY_UNIT);
  tick_timer_service_subscribe(units, tick_handler);
}

static void inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *t = dict_find(iter, MESSAGE_KEY_SHOW_SECONDS);
  if (t) {
    s_show_seconds = t->value->int32 != 0;
    persist_write_bool(KEY_SHOW_SECONDS, s_show_seconds);
    update_tick_subscription();
    layer_mark_dirty(s_time_layer);
  }
}

static void init(void) {
  s_show_seconds = persist_exists(KEY_SHOW_SECONDS)
    ? persist_read_bool(KEY_SHOW_SECONDS) : false;

  time_t now = time(NULL);
  localtime_r(&now, &s_now);

  app_message_open(64, 64);
  app_message_register_inbox_received(inbox_received);

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  update_tick_subscription();
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
```

- [ ] **Step 5: Create `.gitignore`**

```
build/
.pebble-linked-resources
*.pbw
*.pyc
node_modules/
```

- [ ] **Step 6: Init git and npm**

```bash
git init
npm install pebble-clay --save
```

- [ ] **Step 7: Commit**

```bash
git add .
git commit -m "feat: project scaffold with layer structure"
```

---

## Task 2: Fonts

**Files:** `resources/fonts/DSEG7Classic-Regular.ttf`, `resources/fonts/DSEG14Classic-Regular.ttf`

- [ ] **Step 1: Download DSEG fonts (MIT licensed)**

```bash
curl -L https://github.com/keshikan/DSEG/releases/download/v0.46/fonts-DSEG_v046.zip -o /tmp/dseg.zip
unzip /tmp/dseg.zip -d /tmp/dseg/
cp "/tmp/dseg/fonts-DSEG_v046/DSEG7-Classic/DSEG7Classic-Regular.ttf" resources/fonts/
cp "/tmp/dseg/fonts-DSEG_v046/DSEG14-Classic/DSEG14Classic-Regular.ttf" resources/fonts/
ls resources/fonts/
```

Expected:
```
DSEG14Classic-Regular.ttf  DSEG7Classic-Regular.ttf
```

- [ ] **Step 2: Build to verify fonts compile**

```bash
pebble build
```

Expected: Build succeeds. If `RESOURCE_ID_FONT_*` symbols are missing, check that the `"name"` fields in `package.json` media array exactly match.

- [ ] **Step 3: Commit**

```bash
git add resources/fonts/
git commit -m "feat: DSEG7 and DSEG14 bitmap fonts"
```

---

## Task 3: Moon Phase Math

**Files:** `src/c/moon.h`, `src/c/moon.c`, `test/test_moon.c`

- [ ] **Step 1: Create `test/test_moon.c`**

```c
// Host-runnable — no pebble.h needed
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <math.h>

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
```

- [ ] **Step 2: Run test**

```bash
clang -o /tmp/test_moon test/test_moon.c -lm && /tmp/test_moon
```

Expected:
```
New moon:  illum=0.000 day=0 waxing=1
Full moon: illum=1.000 day=14 waxing=0
Today:     illum=X.XXX day=XX waxing=X
All moon tests passed.
```

- [ ] **Step 3: Create `src/c/moon.h`**

```c
#pragma once
#include <pebble.h>

#define LUNAR_CYCLE_DAYS  29.53058867f
#define KNOWN_NEW_MOON_TS 947182440

typedef struct {
  float illumination;  // 0.0 new moon → 1.0 full moon
  int   phase_day;     // 0–29
  bool  waxing;        // true = growing, false = shrinking
} MoonPhase;

MoonPhase moon_calculate_phase(time_t now);
void      moon_draw(GContext *ctx, GRect frame, MoonPhase phase);
```

- [ ] **Step 4: Create `src/c/moon.c`**

```c
#include "moon.h"
#include <math.h>

MoonPhase moon_calculate_phase(time_t now) {
  double days  = (double)(now - KNOWN_NEW_MOON_TS) / 86400.0;
  double phase = days / LUNAR_CYCLE_DAYS;
  phase        = phase - (long)phase;
  if (phase < 0) phase += 1.0;
  float illum  = (float)((1.0 - cos(2.0 * M_PI * phase)) / 2.0);
  return (MoonPhase){
    .illumination = illum,
    .phase_day    = (int)(phase * LUNAR_CYCLE_DAYS),
    .waxing       = phase < 0.5,
  };
}

void moon_draw(GContext *ctx, GRect frame, MoonPhase phase) {
  GPoint center = GPoint(
    frame.origin.x + frame.size.w / 2,
    frame.origin.y + frame.size.h / 2
  );
  int r = (frame.size.h < frame.size.w ? frame.size.h : frame.size.w) / 2 - 2;

  // 1. Dark base
  graphics_context_set_fill_color(ctx, GColorMidnightGreen);
  graphics_fill_circle(ctx, center, r);

  // 2. Lit portion — vertical scanlines within circle
  // Terminator x: right side for waxing, left side for waning
  // waxing:  terminator_x = cx + r*(1 - 2*illum)  [starts at right edge, moves left]
  // waning:  terminator_x = cx - r*(1 - 2*illum)  [starts at left edge, moves right]
  graphics_context_set_stroke_color(ctx, GColorPictonBlue);
  graphics_context_set_stroke_width(ctx, 1);

  int terminator_x = phase.waxing
    ? center.x + (int)((float)r * (1.0f - 2.0f * phase.illumination))
    : center.x - (int)((float)r * (1.0f - 2.0f * phase.illumination));

  int x_start = phase.waxing ? terminator_x    : center.x - r;
  int x_end   = phase.waxing ? center.x + r    : terminator_x;

  for (int x = x_start; x <= x_end; x++) {
    int dx = x - center.x;
    if (dx * dx > r * r) continue;
    int dy = (int)sqrt((float)(r * r - dx * dx));
    graphics_draw_line(ctx, GPoint(x, center.y - dy), GPoint(x, center.y + dy));
  }

  // 3. Meridian lines (globe effect) — ellipses of varying rx
  graphics_context_set_stroke_color(ctx, GColorMidnightGreen);
  graphics_context_set_stroke_width(ctx, 1);
  int rx_vals[] = { r/4, r/2, (r*3)/4 };
  for (int e = 0; e < 3; e++) {
    int rx = rx_vals[e];
    GPoint prev = GPoint(center.x, center.y - r);
    for (int s = 1; s <= 20; s++) {
      int32_t angle = DEG_TO_TRIGANGLE(-90 + s * 9);  // 9° steps = 20 steps / 180°
      int x = center.x + rx * cos_lookup(angle) / TRIG_MAX_RATIO;
      int y = center.y +  r * sin_lookup(angle) / TRIG_MAX_RATIO;
      int dx0 = prev.x - center.x, dy0 = prev.y - center.y;
      int dx1 = x - center.x,      dy1 = y - center.y;
      if (dx0*dx0+dy0*dy0 <= r*r && dx1*dx1+dy1*dy1 <= r*r)
        graphics_draw_line(ctx, prev, GPoint(x, y));
      // Mirror on left side
      int mx = center.x - rx * cos_lookup(angle) / TRIG_MAX_RATIO;
      GPoint lp = GPoint(center.x - (prev.x - center.x), prev.y);
      int ldx0 = lp.x-center.x, ldy0=lp.y-center.y;
      int ldx1 = mx-center.x,   ldy1=y-center.y;
      if (ldx0*ldx0+ldy0*ldy0 <= r*r && ldx1*ldx1+ldy1*ldy1 <= r*r)
        graphics_draw_line(ctx, lp, GPoint(mx, y));
      prev = GPoint(x, y);
    }
  }

  // 4. Center vertical line
  graphics_draw_line(ctx, GPoint(center.x, center.y - r), GPoint(center.x, center.y + r));

  // 5. Blue outer ring
  graphics_context_set_stroke_color(ctx, GColorCobaltBlue);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, center, r);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, center, r + 2);
}
```

- [ ] **Step 5: Build**

```bash
pebble build
```

Expected: Build succeeds.

- [ ] **Step 6: Commit**

```bash
git add src/c/moon.h src/c/moon.c test/test_moon.c
git commit -m "feat: moon phase calculation and globe drawing"
```

---

## Task 4: Tide Math

**Files:** `src/c/tide.h`, `src/c/tide.c`, `test/test_tide.c`

- [ ] **Step 1: Create `test/test_tide.c`**

```c
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
```

- [ ] **Step 2: Run test**

```bash
clang -o /tmp/test_tide test/test_tide.c -lm && /tmp/test_tide
```

Expected:
```
Tide heights: <17 floats in [-1.0, 1.0]>
current=8  All tide tests passed.
```

- [ ] **Step 3: Create `src/c/tide.h`**

```c
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
```

- [ ] **Step 4: Create `src/c/tide.c`**

```c
#include "tide.h"
#include <math.h>

TideData tide_calculate(time_t now) {
  TideData d;
  d.current_idx = TIDE_SAMPLES / 2;
  long window   = 12 * 3600;
  time_t start  = now - window / 2;
  int step      = (int)(window / (TIDE_SAMPLES - 1));
  for (int i = 0; i < TIDE_SAMPLES; i++) {
    double angle = 2.0 * M_PI * (double)(start + i*step - KNOWN_NEW_MOON_TS)
                   / TIDAL_PERIOD_SECONDS;
    d.heights[i] = (float)sin(angle);
  }
  return d;
}

void tide_draw(GContext *ctx, GRect frame, TideData data) {
  int ox = frame.origin.x, oy = frame.origin.y;
  int w  = frame.size.w,   h  = frame.size.h;
  int pad = 4;

  // height val [-1,1] → pixel y (top = high tide, bottom = low tide)
  #define H2Y(v) ((int)(oy + pad + (1.0f - (v)) * 0.5f * (h - pad*2)))

  // Faint grid
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(ox, oy+pad),    GPoint(ox+w, oy+pad));
  graphics_draw_line(ctx, GPoint(ox, oy+h/2),    GPoint(ox+w, oy+h/2));
  graphics_draw_line(ctx, GPoint(ox, oy+h-pad),  GPoint(ox+w, oy+h-pad));

  // H / L labels
  GFont sys = fonts_get_system_font(FONT_KEY_FONT_FALLBACK);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, "H", sys, GRect(ox, oy,     8, 10),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, "L", sys, GRect(ox, oy+h-10, 8, 10),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

  // Wave line
  graphics_context_set_stroke_color(ctx, GColorCobaltBlue);
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 1; i < TIDE_SAMPLES; i++) {
    int x0 = ox + (i-1) * w / (TIDE_SAMPLES-1);
    int x1 = ox +  i    * w / (TIDE_SAMPLES-1);
    graphics_draw_line(ctx, GPoint(x0, H2Y(data.heights[i-1])),
                            GPoint(x1, H2Y(data.heights[i])));
  }

  // Orange dot at current position
  int cx = ox + data.current_idx * w / (TIDE_SAMPLES-1);
  int cy = H2Y(data.heights[data.current_idx]);
  graphics_context_set_fill_color(ctx, GColorChromeYellow);
  graphics_fill_circle(ctx, GPoint(cx, cy), 4);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, GPoint(cx, cy), 4);

  #undef H2Y
}
```

- [ ] **Step 5: Build**

```bash
pebble build
```

Expected: Build succeeds.

- [ ] **Step 6: Commit**

```bash
git add src/c/tide.h src/c/tide.c test/test_tide.c
git commit -m "feat: tide calculation and wave drawing"
```

---

## Task 5: Top and Bottom Bars

**Files:** Modify `src/c/main.c`

- [ ] **Step 1: Replace `top_bar_update` stub**

```c
static void top_bar_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  GFont sm = fonts_get_system_font(FONT_KEY_GOTHIC_09);

  graphics_context_set_fill_color(ctx, COLOR_NAVY);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  graphics_context_set_text_color(ctx, COLOR_LABEL);
  graphics_draw_text(ctx, "< Light", sm,
    GRect(4, 1, 50, b.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, "10 Year Battery", sm,
    GRect(0, 1, b.size.w - 4, b.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}
```

- [ ] **Step 2: Replace `bottom_bar_update` stub**

```c
static void bottom_bar_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  GFont sm = fonts_get_system_font(FONT_KEY_GOTHIC_09);

  graphics_context_set_fill_color(ctx, COLOR_NAVY);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  graphics_context_set_text_color(ctx, COLOR_LABEL);
  graphics_draw_text(ctx, "Illuminator", sm,
    GRect(0, 1, b.size.w, b.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}
```

- [ ] **Step 3: Build + install on emulator**

```bash
pebble build && pebble install --emulator emery
```

Expected: Two dark navy bars at top and bottom, rest of screen is black (other layers not yet drawn).

- [ ] **Step 4: Commit**

```bash
git add src/c/main.c
git commit -m "feat: top and bottom bars"
```

---

## Task 6: Day/Date Layer

**Files:** Modify `src/c/main.c`

- [ ] **Step 1: Replace `day_date_update` stub**

```c
static void day_date_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, COLOR_LCD_BG);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // Bottom divider
  graphics_context_set_stroke_color(ctx, COLOR_NAVY);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(0, b.size.h-1), GPoint(b.size.w, b.size.h-1));

  // Ghost segments
  graphics_context_set_text_color(ctx, COLOR_GHOST);
  graphics_draw_text(ctx, "888", s_font_dseg14_24,
    GRect(6, 2, 90, b.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, "8-88", s_font_dseg14_24,
    GRect(0, 2, b.size.w-6, b.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  // Live text
  graphics_context_set_text_color(ctx, COLOR_DIGIT);
  graphics_draw_text(ctx, DAY_NAMES[s_now.tm_wday], s_font_dseg14_24,
    GRect(6, 2, 90, b.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  char date_buf[8];
  snprintf(date_buf, sizeof(date_buf), "%d-%02d", s_now.tm_mon+1, s_now.tm_mday);
  graphics_draw_text(ctx, date_buf, s_font_dseg14_24,
    GRect(0, 2, b.size.w-6, b.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}
```

- [ ] **Step 2: Build + install**

```bash
pebble build && pebble install --emulator emery
```

Expected: Day row shows e.g. `SUN` left and `4-27` right in DSEG14 with ghost segments.

- [ ] **Step 3: Commit**

```bash
git add src/c/main.c
git commit -m "feat: day and date layer with DSEG14"
```

---

## Task 7: Tide + Moon Layer

**Files:** Modify `src/c/main.c`

- [ ] **Step 1: Replace `tide_moon_update` stub**

```c
static void tide_moon_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, COLOR_LCD_BG);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  graphics_context_set_stroke_color(ctx, COLOR_NAVY);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(0, b.size.h-1), GPoint(b.size.w, b.size.h-1));

  int moon_w = 52;
  int tide_w = b.size.w - moon_w - 6;

  tide_draw(ctx, GRect(4, 4, tide_w, b.size.h - 8), s_tide);
  moon_draw(ctx, GRect(b.size.w - moon_w, 2, moon_w, b.size.h - 4), s_moon);
}
```

- [ ] **Step 2: Build + install**

```bash
pebble build && pebble install --emulator emery
```

Expected: Middle row shows tide wave with orange dot on left, moon globe on right.

- [ ] **Step 3: Commit**

```bash
git add src/c/main.c
git commit -m "feat: tide graph and moon phase layer"
```

---

## Task 8: Time Layer

**Files:** Modify `src/c/main.c`

- [ ] **Step 1: Replace `time_update` stub**

```c
static void time_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, COLOR_LCD_BG);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // Top divider
  graphics_context_set_stroke_color(ctx, COLOR_NAVY);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(b.size.w, 0));

  int hour12 = s_now.tm_hour % 12;
  if (hour12 == 0) hour12 = 12;

  char time_buf[6];
  snprintf(time_buf, sizeof(time_buf), "%02d:%02d", hour12, s_now.tm_min);

  // Ghost
  graphics_context_set_text_color(ctx, COLOR_GHOST);
  graphics_draw_text(ctx, "88:88", s_font_dseg7_46,
    GRect(18, 14, b.size.w - 18, b.size.h),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Live time
  graphics_context_set_text_color(ctx, COLOR_DIGIT);
  graphics_draw_text(ctx, time_buf, s_font_dseg7_46,
    GRect(18, 14, b.size.w - 18, b.size.h),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // AM/PM indicator
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, s_now.tm_hour >= 12 ? "P" : "A", s_font_dseg7_18,
    GRect(2, b.size.h/2 - 10, 16, 20),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Seconds (when enabled)
  if (s_show_seconds) {
    char sec_buf[3];
    snprintf(sec_buf, sizeof(sec_buf), "%02d", s_now.tm_sec);
    graphics_context_set_text_color(ctx, COLOR_GHOST);
    graphics_draw_text(ctx, "88", s_font_dseg7_18,
      GRect(b.size.w - 28, 18, 26, b.size.h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_context_set_text_color(ctx, COLOR_DIGIT);
    graphics_draw_text(ctx, sec_buf, s_font_dseg7_18,
      GRect(b.size.w - 28, 18, 26, b.size.h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}
```

- [ ] **Step 2: Build + install**

```bash
pebble build && pebble install --emulator emery
```

Expected: Large DSEG7 time centered, ghost segments behind, P/A indicator on left.

- [ ] **Step 3: Commit**

```bash
git add src/c/main.c
git commit -m "feat: time layer with DSEG7 and seconds toggle"
```

---

## Task 9: Chamfer Corner Overlay

**Files:** Modify `src/c/main.c`

- [ ] **Step 1: Replace `chamfer_update` stub**

```c
static void chamfer_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int W = b.size.w, H = b.size.h, C = CHAMFER_SIZE;
  graphics_context_set_fill_color(ctx, GColorBlack);

  GPoint tl[3] = {GPoint(0,0),   GPoint(C,0),   GPoint(0,C)};
  GPoint tr[3] = {GPoint(W-C,0), GPoint(W,0),   GPoint(W,C)};
  GPoint bl[3] = {GPoint(0,H-C), GPoint(C,H),   GPoint(0,H)};
  GPoint br[3] = {GPoint(W,H-C), GPoint(W-C,H), GPoint(W,H)};

  GPathInfo pi;
  GPath *p;

  pi = (GPathInfo){3, tl}; p = gpath_create(&pi); gpath_draw_filled(ctx, p); gpath_destroy(p);
  pi = (GPathInfo){3, tr}; p = gpath_create(&pi); gpath_draw_filled(ctx, p); gpath_destroy(p);
  pi = (GPathInfo){3, bl}; p = gpath_create(&pi); gpath_draw_filled(ctx, p); gpath_destroy(p);
  pi = (GPathInfo){3, br}; p = gpath_create(&pi); gpath_draw_filled(ctx, p); gpath_destroy(p);
}
```

- [ ] **Step 2: Build + install**

```bash
pebble build && pebble install --emulator emery
```

Expected: Black triangles clip all four screen corners.

- [ ] **Step 3: Commit**

```bash
git add src/c/main.c
git commit -m "feat: chamfered LCD corners"
```

---

## Task 10: Config Page (Seconds Toggle)

**Files:** `src/pkjs/config.js`, `src/pkjs/index.js`

- [ ] **Step 1: Create `src/pkjs/config.js`**

```js
module.exports = [
  { "type": "heading", "defaultValue": "Casio Waves" },
  {
    "type": "toggle",
    "messageKey": "SHOW_SECONDS",
    "label": "Show Seconds",
    "description": "Ticks every second when on.",
    "defaultValue": false
  },
  { "type": "submit", "defaultValue": "Save" }
];
```

- [ ] **Step 2: Create `src/pkjs/index.js`**

```js
var Clay = require('pebble-clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);
```

- [ ] **Step 3: Build**

```bash
pebble build
```

Expected: Build succeeds.

- [ ] **Step 4: Test settings round-trip**

```bash
pebble install --emulator emery && pebble logs
```

Open the emulator companion app → Settings → toggle "Show Seconds". Verify in `pebble logs` that seconds appear/disappear on the watchface.

- [ ] **Step 5: Commit**

```bash
git add src/pkjs/config.js src/pkjs/index.js
git commit -m "feat: Clay config page for seconds toggle"
```

---

## Task 11: Full Visual Review

- [ ] **Step 1: Build and install**

```bash
pebble build && pebble install --emulator emery
```

- [ ] **Step 2: Visual checklist**

- [ ] Top bar: `< Light` left · `10 Year Battery` right · dark navy bg
- [ ] Day/date: day name left · M-DD right · DSEG14 · ghost segments
- [ ] Tide: wave with orange dot · H/L labels
- [ ] Moon: globe with meridian lines · blue ring · lit side correct for today's phase
- [ ] Time: HH:MM in large DSEG7 · ghost segments · P/A indicator
- [ ] Bottom bar: `Illuminator` centered · dark navy bg
- [ ] Chamfered corners: four black triangles clipping corners
- [ ] All sections separated by dark navy 2px dividers

- [ ] **Step 3: Test seconds toggle end-to-end**

Enable → seconds appear, watch them count. Disable → gone. Kill and relaunch watchface → setting persists.

- [ ] **Step 4: Verify tide refreshes on 15-min boundary**

Add temporary log in `tick_handler`:

```c
APP_LOG(APP_LOG_LEVEL_DEBUG, "Tide refresh at %02d:%02d", tick_time->tm_hour, tick_time->tm_min);
```

Confirm log fires when minute is 0/15/30/45. Remove log.

- [ ] **Step 5: Final commit**

```bash
git add -A
git commit -m "feat: Casio Waves complete"
```

---

## Self-Review

**Spec coverage:**
- ✅ Top bar: `◀ Light` + `10 Year Battery`, minimum-height dark navy
- ✅ Day/date: DSEG14, day + M-DD, ghost segments
- ✅ Tide: sine wave, orange dot, H/L labels, 15-min refresh, pure C math
- ✅ Moon: globe with meridian lines, lit arc ∝ illumination, waxing/waning direction, blue ring
- ✅ Time: DSEG7, HH:MM, ghost segments, P/A indicator
- ✅ Seconds: off by default, toggleable, `SECOND_UNIT` tick when on
- ✅ Bottom bar: `Illuminator` centered, matches top bar height
- ✅ Chamfered screen corners (12px)
- ✅ Dark navy 2px dividers between all rows
- ✅ Seafoam green LCD background throughout
- ✅ No network calls anywhere
- ✅ Settings persisted via `persist_read/write_bool`

**Type consistency:**
- `TideData` and `MoonPhase` defined in `.h` files, used consistently in `main.c`
- `s_font_dseg7_46`, `s_font_dseg7_18`, `s_font_dseg14_24` match `RESOURCE_ID_FONT_*` names in `package.json`
- `tide_draw(ctx, frame, data)` and `moon_draw(ctx, frame, phase)` signatures match across `.h`, `.c`, and call sites
