# Sesame Robot — Technical Documentation

> Firmware for a WiFi-controlled 8-servo quadruped robot with an OLED face display, captive portal web interface, and REST API.  
> Platform: **ESP32-S2** (Lolin S2 Mini) — Framework: **Arduino via PlatformIO**

---

## Table of Contents

1. [Hardware Overview](#1-hardware-overview)
2. [Software Architecture](#2-software-architecture)
3. [Configuration Macros](#3-configuration-macros)
4. [Servo System](#4-servo-system)
5. [Face Animation System](#5-face-animation-system)
6. [Network Stack](#6-network-stack)
7. [HTTP API & Web Interface](#7-http-api--web-interface)
8. [Serial CLI](#8-serial-cli)
9. [Main Loop & State Machine](#9-main-loop--state-machine)
10. [File Reference](#10-file-reference)
11. [Future Development Ideas](#11-future-development-ideas)

---

## 1. Hardware Overview

| Component | Detail |
|---|---|
| MCU | ESP32-S2 (Lolin S2 Mini) |
| Display | SSD1306 128×64 OLED, I²C @ 0x3C |
| I²C pins | SDA = GPIO 33, SCL = GPIO 35 |
| Servo channels | 8 × standard PWM servo (50 Hz, 732–2929 µs pulse width) |
| Servo GPIO pins | 1, 2, 4, 6, 8, 10, 13, 14 |
| Power budget | Must be externally powered; all 8 servos share the ESP32 PWM timers |

### Leg topology

The robot has 4 legs (quadruped), each with 2 degrees of freedom:

```
 Front
  R1 ─ R3       L1 ─ L3
  R2 ─ R4       L2 ─ L4
 Rear
```

Each leg has:
- **Hip servo** (R1/R2/L1/L2) — rotates the leg fore/aft in the horizontal plane
- **Shoulder/knee servo** (R3/R4/L3/L4) — raises and lowers the limb

The logical mapping from name to `servos[]` array index is:

```
R1=0  R2=1  L1=2  L2=3
R4=4  R3=5  L3=6  L4=7
```

This mapping is defined by the `ServoName` enum in `movement-sequences.h`.

---

## 2. Software Architecture

```
src/main.cpp
 ├── WiFi / DNS / mDNS / WebServer
 ├── OLED driver (Adafruit SSD1306)
 ├── Face animation state machine
 ├── Idle blink automaton
 ├── WiFi info scroll overlay
 ├── Command dispatcher (loop)
 └── Serial CLI

include/movement-sequences.h    ← all pose and gait functions (inline)
include/face-bitmaps.h          ← all bitmap data + FACE_LIST macro
include/captive-portal.h        ← index_html PROGMEM string (web UI)
```

The firmware is **single-threaded and cooperative**. There is no RTOS. Concurrency is achieved by:
- Running short, non-blocking tasks in `loop()`.
- Replacing `delay()` with `delayWithFace()`, which keeps the OLED, HTTP server, and DNS server alive while waiting.
- The `pressingCheck()` function polls the current command during multi-step gait cycles, aborting if the command changes.

---

## 3. Configuration Macros

All top-level configuration is in `main.cpp` via `#define`:

| Macro | Default | Purpose |
|---|---|---|
| `AP_SSID` | `"Sesame-Controller-BETA"` | WiFi Access Point name |
| `AP_PASS` | `"12345678"` | AP password (≥ 8 chars required by WPA2) |
| `NETWORK_SSID` | `""` | optional home WiFi SSID for station mode |
| `NETWORK_PASS` | `""` | optional home WiFi password |
| `ENABLE_NETWORK_MODE` | `false` | set `true` to attempt router connection |
| `SCREEN_WIDTH` | `128` | OLED width in pixels |
| `SCREEN_HEIGHT` | `64` | OLED height in pixels |
| `OLED_I2C_ADDR` | `0x3C` | I²C address of SSD1306 |
| `I2C_SDA` | `33` | SDA GPIO for S2 Mini |
| `I2C_SCL` | `35` | SCL GPIO for S2 Mini |

Runtime-tunable parameters (exposed via `/getSettings` / `/setSettings`):

| Variable | Default | Purpose |
|---|---|---|
| `frameDelay` | `100` ms | time between gait keyframes |
| `walkCycles` | `10` | number of full stride cycles per walk command |
| `motorCurrentDelay` | `20` ms | pause inserted after every `setServoAngle()` call to prevent inrush spikes |
| `faceFps` | `8` fps | global default face animation frame rate |

---

## 4. Servo System

### Initialisation

Four ESP32 hardware PWM timers are allocated via `ESP32PWM`:

```cpp
ESP32PWM::allocateTimer(0..3);
```

Each servo is configured for 50 Hz and attached with explicit microsecond bounds:

```cpp
servos[i].setPeriodHertz(50);
servos[i].attach(servoPins[i], 732, 2929);
```

The 732–2929 µs range gives approximately ±90° with a centre at ~1830 µs (90°).

### `setServoAngle(uint8_t channel, int angle)`

The only function that directly moves hardware. It:
1. Adds the per-channel **subtrim** offset: `adjustedAngle = constrain(angle + servoSubtrim[channel], 0, 180)`.
2. Calls `servos[channel].write(adjustedAngle)`.
3. Calls `delayWithFace(motorCurrentDelay)` — keeping the display alive while respecting the inrush delay.

### Subtrim calibration

`int8_t servoSubtrim[8]` stores per-servo angle offsets (range −90 to +90).  
They are applied transparently at every `setServoAngle()` call.  
Via the Serial CLI:

```
subtrim <motor> <value>   # set offset for one servo
subtrim save              # print C array to paste back into source
subtrim reset             # zero all offsets
st <motor> <value>        # short form
```

---

## 5. Face Animation System

### Bitmap format

Every face bitmap is a monochrome 128×64-pixel image stored as a `const unsigned char[]` in PROGMEM (flash), encoded in the standard horizontal-byte, MSB-first convention expected by `Adafruit_GFX::drawBitmap()`.

Each pixel row is 128 bits = 16 bytes. Full frame size: 16 × 64 = **1024 bytes per frame**.

Bitmaps are generated with the [image2cpp](https://javl.github.io/image2cpp/) tool and pasted raw into `face-bitmaps.h`.

### The `FACE_LIST` X-macro

The central registry of all faces is a single macro in `face-bitmaps.h`:

```cpp
#define FACE_LIST \
    X(walk)   X(rest)   X(swim)   X(dance)  X(wave)   \
    X(point)  X(stand)  X(cute)   X(pushup) X(freaky) \
    X(bow)    X(worm)   X(shake)  X(shrug)  X(dead)   \
    X(crab)   X(defualt) X(idle)  X(idle_blink)        \
    X(happy)  X(talk_happy) X(sad)  X(talk_sad)         \
    X(angry)  X(talk_angry) ...
```

This macro is expanded **three times** with different `X()` definitions:

1. **In `face-bitmaps.h`** — declares all `epd_bitmap_<name>[]` symbols as `extern`, weak-linked, so missing bitmaps link to NULL rather than causing a linker error.

2. **In `main.cpp` — `MAKE_FACE_FRAMES` expansion** — for each face name, creates a `const unsigned char* const face_<name>_frames[6]` array pointing to frames `_0` through `_5`:
   ```cpp
   #define MAKE_FACE_FRAMES(name) \
     const unsigned char* const face_##name##_frames[] = { \
       epd_bitmap_##name, epd_bitmap_##name##_1, ..., epd_bitmap_##name##_5 \
     };
   #define X(name) MAKE_FACE_FRAMES(name)
   FACE_LIST
   ```

3. **In `main.cpp` — `faceEntries[]` table** — builds an array of `FaceEntry { name, frames*, maxFrames }` structs used for runtime lookup by string name.

A face can have from 1 to 6 frames (`MAX_FACE_FRAMES = 6`). `countFrames()` stops counting at the first `nullptr` (a missing frame symbol resolves to a weak null pointer).

### Animation modes

Defined by `enum FaceAnimMode`:

| Mode | Value | Behaviour |
|---|---|---|
| `FACE_ANIM_LOOP` | 0 | Cycles frames 0→N→0→N… forever |
| `FACE_ANIM_ONCE` | 1 | Plays frames 0→N, freezes on last frame, sets `faceAnimFinished = true` |
| `FACE_ANIM_BOOMERANG` | 2 | Plays 0→N→0→N… ping-pong, using `faceFrameDirection` (±1) |

### Key functions

| Function | Signature | Purpose |
|---|---|---|
| `setFace()` | `(const String& name)` | Looks up face by name in `faceEntries[]`, resets frame index and timing. If name not found, falls back to `default`. |
| `setFaceMode()` | `(FaceAnimMode mode)` | Changes mode for currently active face, resets direction. |
| `setFaceWithMode()` | `(const String& name, FaceAnimMode mode)` | Atomic: set mode then set face. |
| `updateAnimatedFace()` | `()` | Called every loop iteration. Uses `millis()` to advance the frame index based on `currentFaceFps`. Writes bitmap to OLED via `updateFaceBitmap()`. |
| `updateFaceBitmap()` | `(const unsigned char* bitmap)` | Clears display, draws one 128×64 bitmap, calls `display.display()`. |
| `getFaceFpsForName()` | `(const String& name) → int` | Looks up per-face FPS in `faceFpsEntries[]`. Falls back to global `faceFps`. |

### Idle blink automaton

When the robot enters the idle state (after `runStandPose(1)`), `enterIdle()` is called:
- Sets face to `idle` in `FACE_ANIM_BOOMERANG`.
- Schedules the next blink event in 3–7 seconds (random).

`updateIdleBlink()` runs every loop tick:
1. If `idleBlinkActive == false` and `millis() >= nextIdleBlinkMs`, switches face to `idle_blink` in `FACE_ANIM_ONCE` and optionally queues a double-blink (30% chance).
2. Once `faceAnimFinished == true`, returns to `idle` BOOMERANG and schedules the next blink.

### WiFi info scroll overlay

If no user input has been received in **30 seconds**, `updateWifiInfoScroll()` draws a 10-pixel-high black bar at the top of the OLED and renders horizontally scrolling text (±2 px per 150 ms tick) over the current face background. Text wraps when `wifiScrollPos >= wifiInfoText.length() * 6`. The overlay disappears permanently once the first input is recorded via `recordInput()`.

---

## 6. Network Stack

The firmware supports two simultaneous WiFi roles via `WIFI_AP_STA`:

| Role | When active | IP |
|---|---|---|
| Soft Access Point | Always | 192.168.4.1 (default) |
| Station (STA) | Only if `ENABLE_NETWORK_MODE = true` and SSID configured | DHCP from router |

### Captive Portal (DNS)

`DNSServer dnsServer` on port 53 is started with a wildcard rule: `dnsServer.start(53, "*", myIP)`.  
Every DNS query (from any device that joins the AP) resolves to the ESP32's own IP, forcing the operating system's captive portal detector to open the robot controller page automatically.

### mDNS

`MDNS.begin("sesame-robot")` publishes the device as `sesame-robot.local` on the local network (when in station mode). The HTTP service is also advertised: `MDNS.addService("http", "tcp", 80)`.

---

## 7. HTTP API & Web Interface

All routes are registered in `setup()` and handled by `WebServer server(80)`.

### Routes

| Method | Path | Handler | Description |
|---|---|---|---|
| `GET` | `/` | `handleRoot` | Serves the web UI HTML (from `captive-portal.h`) |
| `GET` | `/cmd?pose=<name>` | `handleCommandWeb` | Sets `currentCommand` to pose name |
| `GET` | `/cmd?go=<name>` | `handleCommandWeb` | Sets `currentCommand` to motion name |
| `GET` | `/cmd?stop` | `handleCommandWeb` | Clears `currentCommand` |
| `GET` | `/cmd?motor=<n>&value=<deg>` | `handleCommandWeb` | Directly drives one servo (1-based index or named: R1, L2, etc.) |
| `GET` | `/getSettings` | `handleGetSettings` | Returns JSON: `frameDelay`, `walkCycles`, `motorCurrentDelay`, `faceFps` |
| `GET` | `/setSettings?<params>` | `handleSetSettings` | Updates runtime parameters |
| `GET` | `/api/status` | `handleGetStatus` | Returns JSON: `currentCommand`, `currentFace`, `networkConnected`, `apIP`, `networkIP` |
| `POST` | `/api/command` | `handleApiCommand` | Accepts JSON body: executes movement and/or changes face |
| `*` | (not found) | `handleRoot` | Captive portal catch-all |

### `POST /api/command` — JSON protocol

The body is parsed manually (no JSON library) with `indexOf()`.  
Supported fields:

```json
{ "command": "forward" }
{ "face": "happy" }
{ "command": "dance", "face": "excited" }
{ "face": "talk_sad" }
```

If only `"face"` is present (no `"command"`), the body is treated as face-only and executed without touching the movement state. This is designed for Python AI orchestration layers that set emotional expressions independently of locomotion.

### Web UI (`captive-portal.h`)

The entire frontend is a single `const char index_html[]` PROGMEM string (~several KB). It contains:
- Motor sliders for all 8 servos.
- Directional pad (forward / backward / left / right).
- Pose buttons for all named animations.
- A settings panel exposing `frameDelay`, `walkCycles`, `motorCurrentDelay`, `faceFps`.

Buttons send simple GET requests to `/cmd`, which return immediately with `"OK"` so the browser UI remains responsive even during long animations.

---

## 8. Serial CLI

The serial CLI runs at **115200 baud** inside `loop()`. It uses a 32-byte static ring buffer. Commands are newline-terminated.

| Command | Short form | Effect |
|---|---|---|
| `run walk` | `rn wf` | Run forward walk |
| `rn wb` | — | Walk backward |
| `rn tl` | — | Turn left |
| `rn tr` | — | Turn right |
| `run rest` | `rn rs` | Rest pose |
| `run stand` | `rn st` | Stand pose |
| `rn wv` | — | Wave |
| `rn dn` | — | Dance |
| `rn sw` | — | Swim |
| `rn pt` | — | Point |
| `rn pu` | — | Pushup |
| `rn bw` | — | Bow |
| `rn ct` | — | Cute |
| `rn fk` | — | Freaky |
| `rn wm` | — | Worm |
| `rn sk` | — | Shake |
| `rn sg` | — | Shrug |
| `rn dd` | — | Dead |
| `rn cb` | — | Crab |
| `<n> <deg>` | — | Set servo n (0–7) to angle deg |
| `all <deg>` | — | Set all 8 servos to same angle |
| `subtrim <n> <v>` | `st <n> <v>` | Set subtrim offset for servo n |
| `subtrim` | `st` | Print all subtrim values |
| `subtrim save` | `st save` | Print C array to paste into source |
| `subtrim reset` | `st reset` | Zero all subtrims |

---

## 9. Main Loop & State Machine

```
loop()
 ├── dnsServer.processNextRequest()   ← captive portal DNS
 ├── server.handleClient()            ← HTTP requests
 ├── updateAnimatedFace()             ← advance OLED frame if due
 ├── updateIdleBlink()                ← idle blink state machine
 ├── updateWifiInfoScroll()           ← WiFi info ticker overlay
 ├── [if currentCommand != ""]        ← dispatch movement
 │    ├── "forward"  → runWalkPose()
 │    ├── "backward" → runWalkBackward()
 │    ├── "left"     → runTurnLeft()
 │    ├── "right"    → runTurnRight()
 │    ├── "rest"     → runRestPose() + clear command
 │    ├── "stand"    → runStandPose(1) + clear command
 │    └── ...        → (all other one-shot poses clear themselves)
 └── [if Serial.available()]          ← serial CLI
```

**Continuous motions** (forward, backward, left, right, wave, dance, crab…) keep `currentCommand` set and re-enter their function on the next loop iteration, creating a continuous loop broken only when `pressingCheck()` detects a command change.

**One-shot poses** (rest, stand, bow, point…) clear `currentCommand` themselves at the end of their function, so the dispatcher does not re-invoke them.

`delayWithFace(ms)` is **the core non-blocking primitive**: it spins for the requested duration while continuously calling `updateAnimatedFace()`, `server.handleClient()`, and `dnsServer.processNextRequest()`. This keeps WiFi responsive and the OLED animating during every inter-keyframe pause.

---

## 10. File Reference

### `src/main.cpp`

| Symbol | Kind | Role |
|---|---|---|
| `currentCommand` | `String` global | The active motion/pose name; `""` = idle |
| `currentFaceName` | `String` global | Name of the active face |
| `currentFaceFrames` | `const unsigned char* const*` | Pointer to active frames array |
| `currentFaceFrameCount` | `uint8_t` | Number of valid frames in current face |
| `currentFaceFrameIndex` | `uint8_t` | Index of currently displayed frame |
| `currentFaceMode` | `FaceAnimMode` | Loop / Once / Boomerang |
| `faceAnimFinished` | `bool` | Set true when ONCE animation completes |
| `idleActive` | `bool` | True when robot is in idle stand state |
| `servoSubtrim[8]` | `int8_t[]` | Per-servo angle trim offsets |
| `frameDelay` | `int` | ms between gait keyframes |
| `walkCycles` | `int` | Stride count per directional command |
| `motorCurrentDelay` | `int` | ms pause after each `setServoAngle()` call |
| `MAKE_FACE_FRAMES(name)` | macro | Builds `face_<name>_frames[6]` pointer arrays |
| `FACE_LIST` | X-macro | Single source of truth for all face names |

### `include/face-bitmaps.h`

| Symbol | Kind | Role |
|---|---|---|
| `FACE_LIST` | X-macro | Registry of all 37 face names |
| `epd_bitmap_<name>[]` | `const unsigned char[]` PROGMEM | Frame 0 for face `<name>` |
| `epd_bitmap_<name>_N[]` | `const unsigned char[]` PROGMEM | Frame N (1–5) for animated faces |
| Weak extern declarations | linker hint | Missing frames resolve to NULL instead of linker error |

### `include/movement-sequences.h`

| Symbol | Kind | Role |
|---|---|---|
| `ServoName` | `enum uint8_t` | Named aliases (R1–R4, L1–L4) for servo indices 0–7 |
| `servoNameToIndex()` | `inline` function | String → servo index lookup |
| `FaceAnimMode` | `enum uint8_t` | Loop / Once / Boomerang |
| `runRestPose()` | `inline void` | Sets all servos to 90° |
| `runStandPose(int face)` | `inline void` | Positions all 8 servos in standing posture; calls `enterIdle()` if face==1 |
| `runWalkPose()` | `inline void` | Forward gait loop, interruptible via `pressingCheck("forward", frameDelay)` |
| `runWalkBackward()` | `inline void` | Reverse gait loop |
| `runTurnLeft/Right()` | `inline void` | Differential turning gait |
| `runWavePose()` | `inline void` | Lifts and oscillates one limb 4 times |
| `runDancePose()` | `inline void` | Rocks knee servos 5 cycles |
| `runSwimPose()` | `inline void` | Alternates hip servos 4 cycles |
| `runPushupPose()` | `inline void` | Extends and retracts knee servos 4 times |
| `runBowPose()` | `inline void` | Lowers front, holds, then stands |
| `runCrabPose()` | `inline void` | Alternates lateral leg sweep 5 cycles |
| `pressingCheck()` | `bool` | Polls `currentCommand` for `ms` ms, aborts gait if changed |

### `include/captive-portal.h`

Contains a single `const char index_html[]` PROGMEM string: the complete web control interface (HTML + CSS + JavaScript).

---

## 11. Future Development Ideas

### A. Logo / custom bitmap on startup

The display system already supports arbitrary 128×64 monochrome bitmaps. To show a logo at boot:

1. Convert your image to a 128×64 monochrome bitmap using [image2cpp](https://javl.github.io/image2cpp/) (horizontal, 1 bit per pixel, MSB first).
2. Paste the `const unsigned char epd_bitmap_logo[]` array into `face-bitmaps.h` (or a new header).
3. Add `X(logo)` to the `FACE_LIST` macro.
4. In `setup()`, after the display initializes, call:
   ```cpp
   setFace("logo");
   delayWithFace(2000);   // show for 2 seconds
   setFace("rest");       // then switch to normal face
   ```

### B. Paired face + motion commands

The architecture is already designed for this: `setFaceWithMode()` and `runXxxPose()` are independent. You can define new combined actions anywhere, for example:

```cpp
// New command: "greet" — wave + happy face simultaneously
inline void runGreetPose() {
  setFaceWithMode("happy", FACE_ANIM_BOOMERANG);
  runWalkPose();        // walk toward something
  runWavePose();        // wave
  runStandPose(1);
}
```

To expose it, add to the `loop()` dispatcher:
```cpp
else if (cmd == "greet") { runGreetPose(); currentCommand = ""; }
```
And add the button in `captive-portal.h`.

### C. New face animations (multi-frame)

Up to 6 frames per face are supported. To add a 3-frame animated expression:
1. Create `epd_bitmap_myface[]`, `epd_bitmap_myface_1[]`, `epd_bitmap_myface_2[]` in `face-bitmaps.h`.
2. Add `X(myface)` to `FACE_LIST`.
3. Add `{ "myface", N_fps }` to `faceFpsEntries[]` in `main.cpp`.
4. Call `setFaceWithMode("myface", FACE_ANIM_LOOP)` from any pose function.

### D. SVG / vector art display

The SSD1306 is a 1-bit, pixel-addressable display — it has no native SVG renderer. To display vector artwork:

1. **Pre-rasterize offline**: Convert your SVG to a 128×64 PNG at 1 bit/pixel, then use image2cpp to produce a PROGMEM array as above. This is the recommended approach.
2. **Runtime rasterization** (advanced): Implement a minimal line-drawing pipeline using `Adafruit_GFX` primitives (`drawLine`, `drawCircle`, `fillTriangle`, etc.) to reconstruct simple SVGs at runtime. Suitable for logos made of basic shapes.

### E. Expanding the REST API

The `/api/command` endpoint currently parses JSON manually. For richer external integration (e.g., a Python AI layer that controls the robot):

- Add a `"face"` + `"command"` combined payload (already supported).
- Add `"subtrim"` parameter to calibrate servos remotely.
- Add `"sequence"` parameter: a JSON array of timed steps `[{command, face, duration_ms}]`.
- Respond with the robot's state (current face, position) in the body.

### F. Persistent configuration (NVS / EEPROM)

Currently, subtrim values and runtime settings (`frameDelay`, `walkCycles`, etc.) are lost on reboot. Use the ESP32 NVS (Non-Volatile Storage) via the `Preferences` library:

```cpp
#include <Preferences.h>
Preferences prefs;
prefs.begin("sesame", false);
prefs.putInt("frameDelay", frameDelay);
frameDelay = prefs.getInt("frameDelay", 100);
```

Store subtrims as a blob: `prefs.putBytes("subtrim", servoSubtrim, 8)`.

### G. Scripted movement sequences

Add a simple sequencer: a queue of `{command, face, delay_ms}` structs processed in `loop()`. Commands could be uploaded via the web interface or `/api/command`, enabling pre-programmed performances or scripted routines triggered remotely.

### H. OTA (Over-the-Air) firmware updates

The ESP32 Arduino core includes `ArduinoOTA`. Add to `setup()`:

```cpp
ArduinoOTA.begin();
```

And in `loop()`:
```cpp
ArduinoOTA.handle();
```

This allows flashing new firmware over WiFi without physical USB access — essential once the robot is assembled.

---

*Generated from source: `src/main.cpp`, `include/movement-sequences.h`, `include/face-bitmaps.h`, `include/captive-portal.h`*
