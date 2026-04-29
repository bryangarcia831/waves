# Casio Waves — Pebble Time 2 (Emery) Watchface Design

**Date:** 2026-04-27  
**Platform:** Pebble Time 2 (Emery) — 200×228px color e-paper display  
**Inspiration:** Casio GWX-5600 G-LIDE / WS-1400H Tide Graph

---

## Overview

A Casio G-LIDE-inspired watchface for Pebble Time 2 featuring a segmented LCD aesthetic, real-time tide graph, and moon phase indicator. The display mimics the look and feel of classic Casio digital watches — seafoam green LCD, dark navy accent bars, 7-segment fonts, and a chamfered screen bezel.

---

## Visual Design

### Watch Shell
- Black rounded-rectangle case, standard Pebble Time 2 shape
- Side buttons on left and right (decorative, no custom action required beyond existing Pebble behavior)
- Corner screws (decorative)

### LCD Screen
- **Background color:** Seafoam green (`#8fbcaa` or closest Pebble palette equivalent)
- **Chamfered corners:** The LCD screen itself has angled corner cuts (~12px), giving the display a beveled inset panel look — not the shell
- **Screen bezel:** Thin dark green border around the chamfered screen

### Section Dividers
- Dark navy blue (`#1a3050`) 2px horizontal dividers between every row
- All content rows share the same seafoam background — no per-section color changes

---

## Layout (top to bottom)

### 1. Top Bar — dark navy, minimum height
- Background: dark navy (`#0b1828`)
- Two labels, vertically centered, same italic serif style:
  - **Left:** `◀ Light`
  - **Right:** `10 Year Battery`
- Font: ~9px italic serif, blue-tinted (`rgba(90,150,255,0.75)`)
- Separated from content below by a 2px navy divider

### 2. Day + Date Row
- **Left:** 3-letter day code (e.g. `SUN`) — DSEG14 / 14-segment display font, ~24px
- **Right:** Date in `M-DD` format (e.g. `4-27`) — DSEG14, ~20px
- Ghost segments visible behind live digits (faint, ~7% opacity)
- Separated below by 2px navy divider

### 3. Tide Graph + Moon Phase Row
- Seafoam background, divided below by 2px navy

#### Tide Graph (left ~2/3 of row)
- **Style:** Smooth sine-wave curve with fill underneath, rendered on a faint grid
- **Grid:** Horizontal lines at H/M/L levels; vertical time divisions
- **Wave color:** Blue (`#2a6aaa`), fill fades to transparent at bottom
- **Current position:** Single orange dot (`#f5a623`) on the wave at current time — no label, no text
- **H/L labels:** Small `H` top-left, `L` bottom-left of graph area
- **Refresh rate:** Every 15 minutes (tide position recalculated, dot repositioned)
- **Tide calculation:** Pure C math based on lunar position — no internet required

#### Moon Phase Indicator (right ~1/3 of row)
- **Style:** Globe with curved meridian lines — like longitude lines on a sphere
- **Shape:** Circle, ~36–46px diameter
- **Border:** Blue ring (`#3a84c8`, 2.5px) with a thinner outer ring (`#1a3060`)
- **Dark base:** Deep navy (`#0d1a28`)
- **Lit portion:** Right side filled with blue radial gradient (`#6ab4e8` → `#1a4480`) representing illuminated portion
- **Meridian lines:** Ellipses of varying x-radius clipped to the circle, dark stroke, ~65% opacity — give 3D sphere appearance
- **Phase logic:** Lit arc width corresponds to current moon illumination percentage (0% = new moon, 100% = full moon). Waxes right→left.

### 4. Time Row
- **`P` indicator:** Small DSEG7, ~10px, dimmed — indicates PM
- **Time:** `HH:MM` in DSEG7 / 7-segment font, ~46px, centered with generous horizontal padding
- **Seconds:** Hidden by default (configurable)
- Ghost segments behind live digits
- Separated below by 2px navy divider

### 5. Bottom Bar — dark navy, minimum height (matches top bar exactly)
- Background: same dark navy as top bar
- Center: `Illuminator` in italic serif, same style/size as top bar labels
- Separated from content above by 2px navy divider

---

## Typography

| Element      | Font           | Size  | Notes                        |
|--------------|----------------|-------|------------------------------|
| Day code     | DSEG14 Classic | 24px  | 14-segment, supports letters |
| Date         | DSEG14 Classic | 20px  | 14-segment                   |
| Time HH:MM   | DSEG7 Classic  | 46px  | 7-segment                    |
| P indicator  | DSEG7 Classic  | 10px  | dimmed                       |
| Bar labels   | Serif italic   | 9px   | Georgia or system serif      |

In the Pebble C build, DSEG7/14 will be replaced with **custom bitmap fonts** embedded in the watchface resources — Pebble does not support TTF at runtime.

---

## Color Palette

| Role                  | Color        |
|-----------------------|--------------|
| LCD background        | `#8fbcaa` (seafoam green) |
| Section dividers      | `#1a3050` (dark navy)     |
| Top/bottom bars       | `#0b1828` (darker navy)   |
| Digit color           | `#0a1a0a` (near black)    |
| Ghost segments        | `rgba(10,26,10,0.07)`     |
| Wave line             | `#2a6aaa` (blue)          |
| Current tide dot      | `#f5a623` (orange)        |
| Moon ring             | `#3a84c8` (blue)          |
| Moon lit side         | `#6ab4e8` → `#1a4480`     |
| Bar label text        | `rgba(90,150,255,0.75)`   |

---

## Configurable Options

| Option          | Default | Notes                              |
|-----------------|---------|------------------------------------|
| Show seconds    | Off     | Adds `SS` in DSEG7 next to time. When enabled, tick timer runs at `SECOND_UNIT` to redraw every second. When disabled, tick timer runs at `MINUTE_UNIT`. |
| Tide refresh    | 15 min  | Tide dot repositioned on the 0/15/30/45 minute mark via `MINUTE_UNIT` tick. |

---

## Platform Constraints (Pebble Time 2 / Emery)

- **Display:** 200×228px, 64-color e-paper
- **Language:** C using Pebble SDK
- **Fonts:** Must be bitmap fonts compiled into resources — no TTF at runtime
- **Animation:** E-paper is not suited for smooth animation; all updates are periodic (tick timer)
- **Tide/moon math:** Must run entirely on-device in C, no network calls
- **Memory:** Watchfaces have limited heap; keep allocations minimal
- **Battery:** Avoid unnecessary redraws; use `tick_timer_service` at 1-minute intervals for time, 15-minute for tide

---

## File Structure (expected)

```
casio-waves/
├── appinfo.json          # Pebble app manifest
├── src/
│   └── main.c            # Watchface entry point, layer setup, tick handler
│   └── tide.c / tide.h   # Tide calculation math
│   └── moon.c / moon.h   # Moon phase calculation
├── resources/
│   └── fonts/            # Compiled bitmap fonts (DSEG7, DSEG14 equivalents)
│   └── images/           # Any static bitmaps if needed
└── wscript               # Pebble build script
```

---

## Out of Scope

- Alarm functionality
- Stopwatch / STW modes
- Connectivity indicators (FLASH, SNZ, SIG, etc.)
- Animated wave
- Any network requests
