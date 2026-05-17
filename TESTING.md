# Sesame MuHack — Manual Hardware Test Guide

This file documents the manual hardware tests to perform on the physical robot
for each refactoring phase. Since compilation is verified at each phase in CI,
these tests are **for hardware-only verification** once you have the robot
available and can flash the firmware.

For each phase, check out the corresponding commit on the `develop` branch and
flash it using PlatformIO (`pio run -e lolin_s2_mini -t upload`).

---

## Phase 0 — Safety Net: Constants Extraction

**What changed**: All `#define` constants and pin/network configuration have
been moved to `include/core/config.h`. The empty scaffold folders
(`application/`, `hal/`) have been deleted. No logic has been modified.

**Expected outcome**: Identical behaviour to the pre-refactor `main` branch.

### Test checklist

1. **Boot display**
   - Power on the robot.
   - The SSD1306 OLED should light up and show boot messages:
     `Sesame MuHack Boot`, `Starting AP...`, `Init servos...`
   - After boot, the `rest` face animation should appear.

2. **Access Point**
   - On your phone or laptop, scan for Wi-Fi networks.
   - The network `Sesame MuHack` must appear.
   - Connect using password `12345678`.
   - The captive portal should auto-open (iOS/Android) or you can navigate to
     `http://192.168.4.1` manually.
   - The MuHack remote control UI must load correctly.

3. **Walk command via web UI**
   - In the web UI, press the forward/walk button.
   - The robot should start the walk gait.
   - The OLED should show the walk face animation.
   - Send a `stop` command; the robot should return to stand and then rest face.

4. **All poses via web UI or terminal**
   - Using the terminal in the web UI or `/cmd?pose=<name>`, trigger each of
     the following: `wave`, `dance`, `swim`, `point`, `pushup`, `bow`, `cute`,
     `freaky`, `worm`, `shake`, `shrug`, `dead`, `crab`.
   - Each should execute its pose and return to rest/stand.

5. **Settings endpoint**
   - Navigate to `http://192.168.4.1/getSettings` — should return valid JSON
     with `frameDelay`, `walkCycles`, `motorCurrentDelay`, `faceFps`.
   - Navigate to `http://192.168.4.1/setSettings?frameDelay=200` — should
     return `OK`.
   - Trigger a walk again; it should be noticeably slower.
   - Reset: `http://192.168.4.1/setSettings?frameDelay=100`.

6. **Idle marquee (30-second wait test)**
   - Boot the robot and do NOT send any command.
   - Wait at least 30 seconds.
   - A scrolling text marquee with the AP SSID, password, and IP should appear
     over the face on the OLED.
   - Send any command; the marquee should disappear.

7. **Hack lock**
   - Open the terminal in the web UI and type `hack` — response should be
     `ACCESS GRANTED`.
   - From a different device/browser (different IP), try to send a command via
     `/cmd?pose=wave` — it should return `403 LOCKED`.
   - Back on the first device, type `muhack` — lock should release.
   - The second device can now send commands successfully.

8. **Serial CLI**
   - Open PlatformIO Serial Monitor at 115200 baud.
   - Type `rn wf` and press Enter → robot walks forward.
   - Type `rn rs` → robot goes to rest.
   - Type `st` → should print all 8 subtrim values (all 0).
   - Type `0 45` → servo 0 should move to 45°.
   - Type `rn st` (run stand) → robot returns to stand position.

---

## Phase 1 — Extract Motors Module

**What changed**:

- `include/motors/servo_driver.h` — `ServoName` and `FaceAnimMode` enums,
  `Motors::init()` and `Motors::setAngle()` declarations, `Motors::subtrim[]`.
- `src/motors/servo_driver.cpp` — all servo hardware logic: PWM timer
  allocation, servo attach sequence, angle writes with subtrim.
- `include/motors/poses.h` — pose/gait function prototypes (declarations only).
- `src/motors/poses.cpp` — all 19 `run*Pose` / `run*Gait` function bodies
  (moved out of the old inline header).
- `include/movement-sequences.h` — **deleted** (fully replaced by the above).
- `src/main.cpp` — servo globals removed, `setServoAngle` replaced by
  `Motors::setAngle`, `servoSubtrim` replaced by `Motors::subtrim`, servo init
  replaced by `Motors::init()`.

**Expected outcome**: Identical behaviour to Phase 0.

### Test checklist

1. **Boot** — identical to Phase 0: OLED boot messages appear, `rest` face
   loads after startup. Serial Monitor should show `Init servos...` then
   `=== Sesame Robot READY ===`.

2. **All static poses** — using the web terminal or `/cmd?pose=<name>`, trigger
   each of the 15 poses: `rest`, `stand`, `wave`, `dance`, `swim`, `point`,
   `pushup`, `bow`, `cute`, `freaky`, `worm`, `shake`, `shrug`, `dead`, `crab`.
   - Each pose must execute and the matching face must appear on the OLED.
   - All poses except `dead` must return the robot to stand/rest.

3. **Locomotion gaits** — trigger `forward`, `backward`, `left`, `right`.
   - Robot must move for the configured number of `walkCycles` then stop.
   - While moving, send a different command (e.g. send `right` during `forward`):
     the robot must abort the current gait **within one frame** (~100 ms) and
     start the new one. This verifies `pressingCheck` still works correctly.

4. **Subtrim CLI (Serial Monitor)** — connect at 115200 baud:
   - Type `st` → should print 8 subtrim values, all `+0`.
   - Type `st 0 10` → servo 0 should shift 10° from its last commanded angle.
   - Type `st save` → Serial output should print the `Motors::subtrim` array.
   - Type `st reset` → all trims back to 0.

5. **Individual motor control via web** — navigate to:
   `http://192.168.4.1/cmd?motor=1&value=45`
   - Servo 0 (R1) should move to 45°.
   - Also test by servo name: `/cmd?motor=R1&value=45`.

---

## Phase 2 — Extract Display Module

**What changed**:

- `include/display/bitmaps.h` — bitmap assets moved here from `include/face-bitmaps.h`
  (old file deleted).
- `include/display/face_engine.h` — `FaceAnimMode` enum (moved from `servo_driver.h`);
  `Display` namespace declarations: `init()`, `setMarqueeText()`, `bootMsg()`, `set()`,
  `setMode()`, `setWithMode()`, `tickFace()`, `tickIdle()`, `tickMarquee()`,
  `enterIdle()`, `exitIdle()`, `notifyInput()`, `currentFaceName`, `faceFps`.
- `src/display/face_engine.cpp` — all display state (face frames, idle FSM, marquee
  scrolling) and all render logic moved out of `main.cpp` into the `Display` namespace.
- `include/motors/servo_driver.h` — `FaceAnimMode` enum removed (lives in
  `face_engine.h` now).
- `src/motors/poses.cpp` — removed face `extern` declarations; added
  `#include "display/face_engine.h"`; all `setFaceWithMode` / `enterIdle` calls
  updated to `Display::setWithMode` / `Display::enterIdle`.
- `src/main.cpp` — removed `Adafruit_SSD1306 display`, all face state globals, all
  face function bodies (setFace, updateAnimatedFace, idle FSM, marquee, …); updated
  `loop()`, `delayWithFace()`, `pressingCheck()`, `recordInput()` to call
  `Display::tick*` / `Display::notifyInput()`; setup() calls `Display::init()` and
  `Display::setMarqueeText()`.

**Expected outcome**: Identical behaviour to Phase 1.

### Test checklist

1. **Boot display** — Power on. OLED must show the same boot messages as before
   (`Sesame MuHack Boot`, `Starting AP...`, `Init servos...`). After boot the
   `rest` face animation must appear.

2. **Idle marquee** — Do NOT send any command for 30 seconds. The scrolling WiFi
   info banner must appear across the top of the OLED face. Send any command via
   the web UI — the banner must disappear immediately.

3. **All static poses** — Trigger each of the 15 poses via the web terminal or
   `/cmd?pose=<name>`. Each pose must show its matching face animation on the OLED
   and return the robot to stand/rest on completion.

4. **Locomotion gaits** — Trigger `forward`, `backward`, `left`, `right`. The
   `walk` face must appear. Interrupt each with a different command mid-gait — the
   robot must abort within one frame and switch face to the new command's face.

5. **Idle blink** — After triggering `stand` (which calls `Display::enterIdle()`),
   wait 3–7 seconds. The OLED must randomly switch to the `idle_blink` face then
   return to `idle`. Occasionally a double-blink sequence occurs.

6. **FPS tuning via settings** — Navigate to
   `http://192.168.4.1/setSettings?faceFps=10` → `OK`. Face animation should
   noticeably speed up on the OLED. Reset: `setSettings?faceFps=1`.

7. **Status endpoint** — `GET /api/status` must return valid JSON with the correct
   `currentFace` value matching whatever face is currently shown.

---

## Phase 3 — Extract Web Module

**Goal:** Move all HTTP/DNS server logic out of `main.cpp` into a `Web::` namespace.

**Files added / changed:**
| File | Change |
|---|---|
| `include/web/web_assets.h` | Moved from `include/captive-portal.h` — PROGMEM HTML/CSS/JS assets |
| `include/web/web_server.h` | New — `Web::` namespace declarations: `init()`, `pump()`, network state externs |
| `src/web/web_server.cpp` | New — all HTTP handler bodies, `WebServer`/`DNSServer` instances, hack-lock state, mDNS init |
| `src/main.cpp` | Removed HTTP handlers, `dnsServer`/`server` instances, network/hack-lock globals; setup() calls `Web::init()`; loop()/helpers call `Web::pump()` |

**Design decisions:**

- `Web::networkConnected`, `Web::networkIP`, `Web::deviceHostname` are defined in `web_server.cpp` and set in `setup()` after STA negotiation.
- `currentCommand`, `frameDelay`, `walkCycles`, `motorCurrentDelay` remain in `main.cpp`; `web_server.cpp` picks them up via `extern`.
- HTTP handlers call `Display::notifyInput()` directly — `recordInput()` wrapper no longer needed by the web layer (kept in `main.cpp` for the serial CLI only).
- `hackLocked`, `hackOwnerIP`, `hackOwnerMAC` are now `static` inside `web_server.cpp` — fully private to the web module.
- mDNS responder start moved into `Web::init()` to keep all network service bringup in one place.

**Verification:**

```
pio run   →  SUCCESS  RAM 17.0%  Flash 71.4%
```

**Commit:** _to be filled_

---

## Phase 4 — FreeRTOS Queue Plumbing

**Goal:** Introduce a FreeRTOS queue between web/serial producers and the motor
dispatcher consumer, without yet creating separate RTOS tasks. `loop()` remains
the cooperative scheduler.

**Files added / changed:**
| File | Change |
|---|---|
| `include/core/command_queue.h` | New — `CmdQueue::` namespace: `init()`, `push()`, `pop()` |
| `src/core/command_queue.cpp` | New — FreeRTOS `QueueHandle_t` wrapper (char-array items, depth 4) |
| `src/web/web_server.cpp` | All `currentCommand = X` writes replaced with `CmdQueue::push(X)`; `currentCommand` kept as read-only extern for status reporting |
| `src/main.cpp` | Added `#include "core/command_queue.h"`; `CmdQueue::init()` at end of `setup()`; `loop()` calls `CmdQueue::pop()` to update `currentCommand` before dispatching |

**Design decisions:**
- Queue depth = 4, items are `char[32]` (no heap allocation in HTTP callback context).
- Queue is **last-write-wins**: if full, oldest item is dropped and new one enqueued.
  This preserves the original `currentCommand = cmd` semantics.
- `currentCommand` (in `main.cpp`) is still updated by the dispatcher — it now reflects
  what the motor is actually executing. Web handlers read it as `extern` for status endpoints.
- Serial CLI commands still write `currentCommand` directly (same execution context, no concurrency).

**Verification:**
```
pio run   →  SUCCESS  RAM 17.0%  Flash 71.5%
```

**Commit:** _to be filled_

---

## Phase 5 — Split into 3 FreeRTOS Tasks

_(To be filled after Phase 5 implementation)_

---

## Phase 6 — Thread-Safety Hardening

_(To be filled after Phase 6 implementation)_
