# Sesame MuHack — Manual Hardware Test Guide

This file documents the manual hardware tests to perform on the physical robot
for each refactoring phase. Since compilation is verified at each phase in CI,
these tests are **for hardware-only verification** once you have the robot
available and can flash the firmware.

For each phase, check out the corresponding commit on the `develop` branch and
flash it using PlatformIO (`pio run -e lolin_s2_mini -t upload`).

---

## How to interact with the robot

There are four interaction surfaces. All of them are available from Phase 0 onward
unless noted otherwise.

---

### 1 — Serial CLI (PlatformIO Serial Monitor)

Open the monitor at **115200 baud** (`pio device monitor` or the PlatformIO IDE
panel). Commands are terminated with `\n` or `\r\n`. The robot prints all
received commands and debug output to the same port.

#### Movement commands

| Command     | What the robot does                       | OLED face |
| ----------- | ----------------------------------------- | --------- |
| `rn wf`     | Walk forward (`walkCycles` repetitions)   | `walk`    |
| `run walk`  | Same as above                             | `walk`    |
| `rn wb`     | Walk backward                             | `walk`    |
| `rn tl`     | Turn left                                 | `walk`    |
| `rn tr`     | Turn right                                | `walk`    |
| `rn rs`     | Rest pose (lies flat)                     | `rest`    |
| `run rest`  | Same as above                             | `rest`    |
| `rn st`     | Stand pose (neutral upright), enters idle | `idle`    |
| `run stand` | Same as above                             | `idle`    |

#### Pose commands

| Command | Pose executed | OLED face |
| ------- | ------------- | --------- |
| `rn wv` | Wave          | `wave`    |
| `rn dn` | Dance         | `dance`   |
| `rn sw` | Swim          | `swim`    |
| `rn pt` | Point         | `point`   |
| `rn pu` | Push-up       | `pushup`  |
| `rn bw` | Bow           | `bow`     |
| `rn ct` | Cute          | `cute`    |
| `rn fk` | Freaky        | `freaky`  |
| `rn wm` | Worm          | `worm`    |
| `rn sk` | Shake         | `shake`   |
| `rn sg` | Shrug         | `shrug`   |
| `rn dd` | Dead          | `dead`    |
| `rn cb` | Crab          | `crab`    |

#### Subtrim calibration commands

Subtrim compensates for mechanical offset in each servo (range −90…+90 degrees).

| Command    | Expected output                                                                                                                             |
| ---------- | ------------------------------------------------------------------------------------------------------------------------------------------- |
| `st`       | `Subtrim values:` then 8 lines: `Motor 0: +0` … `Motor 7: +0`                                                                               |
| `subtrim`  | Same as `st`                                                                                                                                |
| `st 2 5`   | `Motor 2 subtrim set to +5`; servo 2 physically shifts 5°                                                                                   |
| `st 2 -10` | `Motor 2 subtrim set to -10`                                                                                                                |
| `st 2 91`  | `Subtrim value must be between -90 and +90` (no change)                                                                                     |
| `st 9 0`   | `Invalid motor number (0-7)` (no change)                                                                                                    |
| `st save`  | `Copy and paste this into your code:` then `int8_t Motors::subtrim[SERVO_COUNT] = {0, 0, 5, 0, 0, 0, 0, 0};` (values reflect current trims) |
| `st reset` | `All subtrim values reset to 0`; motors snap back to uncompensated angles                                                                   |

#### Individual servo commands

| Command  | Expected output                                                |
| -------- | -------------------------------------------------------------- |
| `0 90`   | `Servo 0 set to 90`; servo 0 (R1) moves to 90°                 |
| `7 45`   | `Servo 7 set to 45`; servo 7 (L4) moves to 45°                 |
| `8 45`   | `Invalid motor number (0-7)`                                   |
| `all 90` | All 8 servos move to 90°; no Serial echo line (silent command) |

---

### 2 — Captive Portal Web UI

Connect to Wi-Fi **`Sesame MuHack`** (password `12345678`). A captive portal
page opens automatically on iOS / Android. On desktops navigate to
**`http://192.168.4.1`**.

The page contains:

- A **D-pad** for directional movement (forward / backward / left / right / stop).
- A **poses grid** with one button per pose.
- A **settings panel** for `frameDelay`, `walkCycles`, `motorCurrentDelay`, `faceFps`.
- An **embedded terminal** for text commands (same command set as the web terminal endpoint below).

Pressing any button or sending any terminal command dismisses the idle WiFi
marquee on the OLED and resets the idle timer.

---

### 3 — HTTP API (curl / browser)

All URLs are relative to `http://192.168.4.1`. On a joined network the robot
also responds on `http://sesame-robot.local` (mDNS).

#### `GET /cmd` — legacy URL-encoded command

| Parameter                 | Value    | Response     | Effect                                     |
| ------------------------- | -------- | ------------ | ------------------------------------------ |
| `pose=wave`               | string   | `OK`         | Executes wave pose                         |
| `pose=forward`            | string   | `OK`         | Starts walk forward                        |
| `go=backward`             | string   | `OK`         | Starts walk backward                       |
| `stop=1`                  | any      | `OK`         | Halts all movement (`currentCommand = ""`) |
| `motor=1&value=90`        | int/int  | `OK`         | Moves servo 1 (R1) to 90°                  |
| `motor=5&value=45`        | int/int  | `OK`         | Moves servo 5 to 45°                       |
| `motor=R1&value=90`       | name/int | `OK`         | Moves servo R1 to 90° by symbolic name     |
| _(hack locked, wrong IP)_ | any      | `403 LOCKED` | Request denied                             |

```bash
# Examples
curl "http://192.168.4.1/cmd?pose=wave"         # → OK
curl "http://192.168.4.1/cmd?go=forward"        # → OK
curl "http://192.168.4.1/cmd?stop=1"            # → OK
curl "http://192.168.4.1/cmd?motor=1&value=90"  # → OK
```

#### `GET /terminal?cmd=<command>` — JSON terminal endpoint

All commands are case-insensitive. Returns `application/json`.

| Command                           | JSON response                                                                                                                                      | Notes                        |
| --------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------- |
| `help`                            | `{"response":"=== SESAME TERMINAL ===\nMovement: forward, backward, left, right, stop\n..."}`                                                      | Lists all available commands |
| `status`                          | `{"response":"IP: 192.168.4.1\nSSID: Sesame MuHack\nClients: 1\nCommand: idle\nFace: rest\nHack Lock: off"}`                                       | Live status snapshot         |
| `forward`                         | `{"response":"Executing: forward"}`                                                                                                                | Starts walk forward          |
| `stop`                            | `{"response":"All movement stopped."}`                                                                                                             | Halts all movement           |
| `wave`                            | `{"response":"Pose: wave"}`                                                                                                                        | Queues wave pose             |
| `hack`                            | `{"response":"ACCESS GRANTED. You now have exclusive control.\nAll other users are locked out.\nType 'muhack' to release control.","locked":true}` | Locks robot to this IP       |
| `muhack`                          | `{"response":"LOCK RELEASED. All users can now control the robot.\nMuHack - Brescia Hackerspace","locked":false}`                                  | Releases hack lock           |
| `muhack` _(not owner)_            | `{"response":"ERROR: Only the hacker who locked can release.\nAsk them to type 'muhack'.","locked":true}`                                          |                              |
| `muhack` _(no lock active)_       | `{"response":"Robot is already free. No lock active.\nMuHack - Brescia Hackerspace","locked":false}`                                               |                              |
| _(any command, locked, wrong IP)_ | `{"response":"ACCESS DENIED. Robot is under exclusive control.\nWait for 'muhack' to be issued.","locked":true}`                                   |                              |
| `xyz`                             | `{"response":"Unknown command: xyz\nType 'help' for available commands."}`                                                                         | Unknown command              |

```bash
curl "http://192.168.4.1/terminal?cmd=status"
curl "http://192.168.4.1/terminal?cmd=hack"
curl "http://192.168.4.1/terminal?cmd=wave"
curl "http://192.168.4.1/terminal?cmd=forward"
curl "http://192.168.4.1/terminal?cmd=stop"
```

#### `GET /getSettings` — read tunable parameters

```bash
curl "http://192.168.4.1/getSettings"
```

Expected response:

```json
{ "frameDelay": 100, "walkCycles": 3, "motorCurrentDelay": 5, "faceFps": 1 }
```

#### `GET /setSettings` — update tunable parameters

All parameters are optional; only the supplied ones are modified.

| Parameter           | Default | Effect                                                    |
| ------------------- | ------- | --------------------------------------------------------- |
| `frameDelay`        | `100`   | Inter-frame pause in ms. Higher = slower gaits.           |
| `walkCycles`        | `3`     | Number of gait repetitions per walk/turn command.         |
| `motorCurrentDelay` | `5`     | Pause after each servo write (ms) to spread current draw. |
| `faceFps`           | `1`     | Default OLED animation frame-rate (fps).                  |

```bash
curl "http://192.168.4.1/setSettings?frameDelay=200"        # → OK  (slower gaits)
curl "http://192.168.4.1/setSettings?faceFps=5"             # → OK  (faster face anim)
curl "http://192.168.4.1/setSettings?walkCycles=5&frameDelay=80"  # → OK
```

After `setSettings?frameDelay=200`, run a walk command — each frame should take
roughly twice as long as the default.

#### `GET /api/status` — machine-readable status

```bash
curl "http://192.168.4.1/api/status"
```

Expected response (while robot is idle):

```json
{
  "currentCommand": "",
  "currentFace": "rest",
  "networkConnected": false,
  "apIP": "192.168.4.1"
}
```

Expected response while executing `wave`:

```json
{
  "currentCommand": "wave",
  "currentFace": "wave",
  "networkConnected": false,
  "apIP": "192.168.4.1"
}
```

#### `POST /api/command` — JSON command endpoint

Body must be `application/json`.

```bash
# Send a pose command
curl -X POST http://192.168.4.1/api/command \
  -H "Content-Type: application/json" \
  -d '{"command":"dance"}'
# → {"status":"ok","message":"Command executed"}

# Send a stop
curl -X POST http://192.168.4.1/api/command \
  -H "Content-Type: application/json" \
  -d '{"command":"stop"}'
# → {"status":"ok","message":"Command stopped"}

# Set face without movement
curl -X POST http://192.168.4.1/api/command \
  -H "Content-Type: application/json" \
  -d '{"face":"happy"}'
# → {"status":"ok","message":"Face updated"}

# Wrong HTTP method
curl http://192.168.4.1/api/command
# → 405  {"error":"Method not allowed"}
```

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
   - Using the terminal in the web UI or `/terminal?cmd=<name>`, trigger each of
     the following: `wave`, `dance`, `swim`, `point`, `pushup`, `bow`, `cute`,
     `freaky`, `worm`, `shake`, `shrug`, `dead`, `crab`.
   - Each should execute its pose and return to rest/stand.

5. **Settings endpoint**
   - `GET /getSettings` — must return JSON with all four fields.
   - `GET /setSettings?frameDelay=200` — must return `OK`.
   - Trigger a walk; it should be noticeably slower.
   - Reset: `GET /setSettings?frameDelay=100`.

6. **Idle marquee (30-second wait test)**
   - Boot the robot and do NOT send any command.
   - Wait at least 30 seconds.
   - A scrolling text marquee with the AP SSID, password, and IP should appear
     over the face on the OLED.
   - Send any command; the marquee should disappear.

7. **Hack lock**
   - Open the terminal in the web UI and type `hack` — response should be
     `ACCESS GRANTED`.
   - From a different device/browser (different IP), try `/terminal?cmd=wave` —
     response should contain `ACCESS DENIED`.
   - Back on the first device, type `muhack` — lock should release.
   - The second device can now send commands successfully.

8. **Serial CLI**
   - Open PlatformIO Serial Monitor at 115200 baud.
   - Type `rn wf` → robot walks forward, Serial prints `[DEBUG] TaskMotor: dispatching command -> forward`.
   - Type `rn rs` → robot rests.
   - Type `st` → 8 lines `Motor N: +0`.
   - Type `0 45` → `Servo 0 set to 45`; servo 0 moves to 45°.
   - Type `rn st` → robot returns to stand.

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

2. **All static poses** — using the web terminal or `/terminal?cmd=<name>`, trigger
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
   - Type `st save` → Serial output should print the `Motors::subtrim` array
     initialiser.
   - Type `st reset` → all trims back to 0.

5. **Individual motor control via web** — navigate to:
   - `http://192.168.4.1/cmd?motor=1&value=45` → `OK`, servo 1 (R1) moves to 45°.
   - `http://192.168.4.1/cmd?motor=R1&value=45` → `OK`, same servo by name.

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
   `/terminal?cmd=<name>`. Each pose must show its matching face animation on the OLED
   and return the robot to stand/rest on completion.

4. **Locomotion gaits** — Trigger `forward`, `backward`, `left`, `right`. The
   `walk` face must appear. Interrupt each with a different command mid-gait — the
   robot must abort within one frame and switch face to the new command's face.

5. **Idle blink** — After triggering `stand` (which calls `Display::enterIdle()`),
   wait 3–7 seconds. The OLED must randomly switch to the `idle_blink` face then
   return to `idle`. Occasionally a double-blink sequence occurs.

6. **FPS tuning via settings** — `GET /setSettings?faceFps=10` → `OK`. Face
   animation should noticeably speed up on the OLED. Reset: `setSettings?faceFps=1`.

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

**Commit:** `1364365`

### Test checklist

1. **Boot + AP** — same as previous phases. Serial Monitor must print `mDNS responder started`.

2. **All HTTP endpoints** — verify every route listed in the _How to interact_ section
   above returns the expected response.

3. **Hack lock flow**:

   ```bash
   curl "http://192.168.4.1/terminal?cmd=hack"
   # → {"response":"ACCESS GRANTED...","locked":true}

   # From a second IP (e.g. another device or curl --interface):
   curl "http://192.168.4.1/terminal?cmd=wave"
   # → {"response":"ACCESS DENIED...","locked":true}

   curl "http://192.168.4.1/terminal?cmd=muhack"
   # → {"response":"ERROR: Only the hacker...","locked":true}

   # Back on the original IP:
   curl "http://192.168.4.1/terminal?cmd=muhack"
   # → {"response":"LOCK RELEASED...","locked":false}
   ```

4. **Status during command** — start a walk (`/terminal?cmd=forward`), then
   immediately poll `/api/status`. `currentCommand` should be `"forward"` and
   `currentFace` should be `"walk"`.

5. **mDNS** — while connected to the same network as the robot's STA interface,
   ping `sesame-robot.local` — should resolve. (Only if `ENABLE_NETWORK_MODE=1`
   in `config.h`.)

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

**Commit:** `7975193`

### Test checklist

1. **Rapid command spam** — send 6+ commands in quick succession via curl:

   ```bash
   for cmd in forward backward left right wave dance; do
     curl -s "http://192.168.4.1/terminal?cmd=$cmd" &
   done
   wait
   ```

   The robot must not crash or lock up. It may skip some intermediate commands
   (queue overflow is expected — last-write-wins), but must always end up
   executing one of them and return to a stable state.

2. **Queue saturation test** — send 4 pose commands before the first one finishes.
   The robot must execute the last received command after completing the current one,
   without hanging.

3. **Status accuracy** — poll `/api/status` every 500 ms while triggering poses.
   `currentCommand` must transition correctly: `""` → `"wave"` → `""`.

4. **All existing tests** — repeat Phase 3 checklist; behaviour must be identical.

---

## Phase 5 — Split into 3 FreeRTOS Tasks

**Goal:** Replace the cooperative `loop()` with three dedicated FreeRTOS tasks.
`loop()` now yields forever; web/display/motor logic each runs in its own task.

**Files added / changed:**

| File                          | Change                                                                                                                                                                                                      |
| ----------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `include/core/tasks.h`        | New — `Tasks::startAll()`, `delayWithFace()`, `pressingCheck()` declarations                                                                                                                                |
| `src/core/tasks.cpp`          | New — `taskWeb` (p1/8KB), `taskDisplay` (p1/4KB), `taskMotor` (p2/8KB); `delayWithFace()` → `vTaskDelay`; `pressingCheck()` → `CmdQueue::pop()` + `vTaskDelay(5)`                                           |
| `src/main.cpp`                | Added `#include "core/tasks.h"`; removed `delayWithFace`/`pressingCheck`/`recordInput` implementations; `loop()` → `vTaskDelay(portMAX_DELAY)`; `setup()` calls `CmdQueue::init()` then `Tasks::startAll()` |
| `src/motors/poses.cpp`        | Replaced `extern void delayWithFace` + `extern bool pressingCheck` inline decls with `#include "core/tasks.h"`                                                                                              |
| `src/motors/servo_driver.cpp` | Replaced `extern void delayWithFace` inline decl with `#include "core/tasks.h"`                                                                                                                             |

**Task design:**

| Task          | Priority | Stack | Body                                                             |
| ------------- | -------- | ----- | ---------------------------------------------------------------- |
| `taskWeb`     | 1        | 8 KB  | `Web::pump()` + `vTaskDelay(1 ms)`                               |
| `taskDisplay` | 1        | 4 KB  | `Display::tickFace/Idle/Marquee()` + `vTaskDelay(10 ms)`         |
| `taskMotor`   | 2        | 8 KB  | `CmdQueue::pop()` + dispatcher + serial CLI + `vTaskDelay(1 ms)` |

**Design decisions:**

- `delayWithFace(ms)` → `vTaskDelay(pdMS_TO_TICKS(ms))`: web and display are driven
  by their own tasks, so spin-polling is no longer needed inside pose/gait routines.
- `pressingCheck(cmd, ms)` polls `CmdQueue::pop()` every 5 ms instead of checking
  `currentCommand` directly; ensures interrupt-responsiveness inside long gaits.
- `currentCommand` remains a plain global in `main.cpp` for Phase 5; reads/writes are
  still unprotected — that is addressed in Phase 6.
- `recordInput()` is inlined as `Display::notifyInput()` inside `tasks.cpp`.
- `frameDelay`, `walkCycles`, `motorCurrentDelay` stay as `extern` globals for Phase 5.
- Arduino `loop()` uses `vTaskDelay(portMAX_DELAY)` — the standard pattern for handing
  full control to FreeRTOS tasks on Arduino-ESP32.

**Verification:**

```
pio run   →  SUCCESS  RAM 17.0%  Flash 71.5%
```

**Commit:** `30f2cee`

### Test checklist

1. **Web responsiveness during long pose** — start the `dance` or `worm` pose (both
   run many cycles). While it is executing, poll `/api/status` or refresh the web UI.
   - In Phase 4 (cooperative), the HTTP server was starved during pose execution —
     requests would time out or stall.
   - In Phase 5 (TaskWeb runs independently), the status endpoint must respond
     immediately even while the robot is mid-gait. Response time should be < 50 ms.

   ```bash
   curl "http://192.168.4.1/terminal?cmd=dance"   # triggers multi-second pose
   # within 1 second, in a separate terminal:
   curl "http://192.168.4.1/api/status"
   # must return JSON instantly, not hang
   ```

2. **OLED continuity during web request** — while the robot is idle and the face
   animation is running, rapidly hammer the web UI. The OLED face animation must
   continue ticking smoothly — it must not freeze or stutter during HTTP traffic
   (TaskDisplay runs at priority 1 independently of TaskWeb).

3. **Interrupt responsiveness** — send `forward`, then immediately send `right` within
   200 ms. `pressingCheck()` now polls the queue every 5 ms, so the robot must abort
   the forward gait and pivot right within one frame (≤ `frameDelay` ms, default 100 ms).

4. **Serial CLI still works** — with TaskMotor owning the serial buffer, all serial CLI
   commands from the _How to interact_ section above must still function correctly.

5. **No reboot / stack overflow** — after running the robot for 5+ minutes with
   mixed web and serial commands, the device must not reboot. A stack overflow would
   appear in Serial Monitor as a `Guru Meditation Error` with a task name.
   If this occurs, increase the affected task's stack constant in `tasks.cpp`.

6. **All Phase 4 tests** — repeat; behaviour must be identical.

---

## Phase 6 — Thread-Safety Hardening

**Goal:** Eliminate data races between the three FreeRTOS tasks on all shared
state variables identified in PLAN.md.

**Files added / changed:**

| File                            | Change                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| ------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `include/display/face_engine.h` | Removed `extern String currentFaceName` and `extern int faceFps`; added `getCurrentFaceName()`, `getFaceFps()`, `setFaceFps(int)` thread-safe accessor declarations                                                                                                                                                                                                                                                                      |
| `src/display/face_engine.cpp`   | Added `#include <freertos/semphr.h>`; `s_currentFaceName` and `s_faceFps` made private static; mutex `s_faceNameMutex` (SemaphoreHandle_t) for String; spinlock `s_faceMux` (portMUX_TYPE) for int; mutex created in `Display::init()`; `Display::set()` reads/writes name under mutex; `getFaceFpsForName()` and `tickFace()` read faceFps under spinlock; accessors `getCurrentFaceName()`, `getFaceFps()`, `setFaceFps()` implemented |
| `src/main.cpp`                  | Added `#include <atomic>`; `frameDelay`, `walkCycles`, `motorCurrentDelay` changed from `int` to `std::atomic<int>`                                                                                                                                                                                                                                                                                                                      |
| `src/web/web_server.cpp`        | Added `#include <atomic>`; updated extern declarations to `std::atomic<int>`; replaced `Display::currentFaceName` reads with `Display::getCurrentFaceName()`; replaced `Display::faceFps` read/write with `Display::getFaceFps()` / `Display::setFaceFps()`                                                                                                                                                                              |
| `src/motors/poses.cpp`          | Updated `extern int frameDelay/walkCycles` to `extern std::atomic<int>`                                                                                                                                                                                                                                                                                                                                                                  |
| `src/motors/servo_driver.cpp`   | Updated `extern int motorCurrentDelay` to `extern std::atomic<int>`                                                                                                                                                                                                                                                                                                                                                                      |
| `src/core/tasks.cpp`            | Removed unused `extern int` declarations for timing params                                                                                                                                                                                                                                                                                                                                                                               |

**Protection strategy:**

| Shared variable                                 | Writers                                  | Readers                                          | Mechanism                                                                         |
| ----------------------------------------------- | ---------------------------------------- | ------------------------------------------------ | --------------------------------------------------------------------------------- |
| `currentCommand`                                | TaskMotor (queue pop, poses, serial CLI) | TaskWeb (status API)                             | FreeRTOS queue (Phase 4); read race accepted (String assign is fast, status-only) |
| `s_currentFaceName`                             | TaskMotor (`Display::set()`)             | TaskWeb (`handleGetStatus`, `handleTerminalCmd`) | `SemaphoreHandle_t` mutex — String heap ops require proper mutex, not spinlock    |
| `s_faceFps`                                     | TaskWeb (`handleSetSettings`)            | TaskDisplay (`tickFace` fallback)                | `portENTER_CRITICAL` spinlock — int read/write, well under 1 µs                   |
| `frameDelay`, `walkCycles`, `motorCurrentDelay` | TaskWeb (`handleSetSettings`)            | TaskMotor (poses, servo_driver)                  | `std::atomic<int>` — hardware-atomic on Xtensa 32-bit + compiler fence            |

**Design decisions:**

- `Display::set()` takes the mutex only for the short String copy at the start
  (idempotency check) and at the end (write resolved name). The heavy face
  lookup and I2C bitmap update happen outside the lock, keeping contention
  minimal.
- `portENTER_CRITICAL` is used only for bare `int` operations (< 1 µs).
  `String` operations (heap alloc) are never inside a critical section.
- `std::atomic<int>` with default `seq_cst` ordering is transparent to all
  callers — `operator int()` and `operator=(int)` make all existing read/write
  sites compile unchanged.
- `currentCommand` read in `handleGetStatus` / `handleTerminalCmd` is not
  explicitly locked — this reflects the plan's note that it is "already
  resolved via queue" for the write path. The status read is best-effort.

**Verification:**

```
pio run   →  SUCCESS  RAM 17.0%  Flash 71.6%
```

**Commit:** `ce6a435`

### Test checklist

1. **Concurrent settings + gait** — while the robot is executing a long gait
   (`dance`, `worm`), hammer the settings endpoint from a browser loop:

   ```bash
   while true; do
     curl -s "http://192.168.4.1/setSettings?frameDelay=80"
     curl -s "http://192.168.4.1/setSettings?frameDelay=150"
   done
   ```

   The robot must not crash. `frameDelay` changes should take effect within the
   next frame (gait visibly speeds up and slows down). No `Guru Meditation Error`.

2. **Status face name race** — rapidly alternate between two poses while polling
   `/api/status`:

   ```bash
   while true; do
     curl -s "http://192.168.4.1/terminal?cmd=wave"
     curl -s "http://192.168.4.1/terminal?cmd=dance"
   done
   ```

   In a second terminal, poll status continuously:

   ```bash
   while true; do curl -s "http://192.168.4.1/api/status"; sleep 0.1; done
   ```

   The `currentFace` field must always be a valid face name string (never
   garbled/empty/corrupted) — the mutex prevents partial String reads.

3. **faceFps race** — while `taskDisplay` is ticking at 1 fps (slow, visible):

   ```bash
   # Set faceFps to 10 then back to 1 rapidly (spinlock test)
   for i in $(seq 1 100); do
     curl -s "http://192.168.4.1/setSettings?faceFps=10" &
     curl -s "http://192.168.4.1/setSettings?faceFps=1" &
   done
   wait
   ```

   Robot must remain stable; OLED must continue animating.

4. **`getSettings` consistency** — call `setSettings?frameDelay=42` then
   immediately `getSettings`. The returned `frameDelay` must be `42`, never an
   intermediate torn value.

   ```bash
   curl "http://192.168.4.1/setSettings?frameDelay=42" && \
   curl "http://192.168.4.1/getSettings"
   # → {"frameDelay":42,"walkCycles":3,"motorCurrentDelay":5,"faceFps":1}
   ```

5. **All Phase 5 tests** — repeat; behaviour must be identical.

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

**Goal:** Replace the cooperative `loop()` with three dedicated FreeRTOS tasks.
`loop()` now yields forever; web/display/motor logic each runs in its own task.

**Files added / changed:**

| File                          | Change                                                                                                                                                                                                      |
| ----------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `include/core/tasks.h`        | New — `Tasks::startAll()`, `delayWithFace()`, `pressingCheck()` declarations                                                                                                                                |
| `src/core/tasks.cpp`          | New — `taskWeb` (p1/8KB), `taskDisplay` (p1/4KB), `taskMotor` (p2/8KB); `delayWithFace()` → `vTaskDelay`; `pressingCheck()` → `CmdQueue::pop()` + `vTaskDelay(5)`                                           |
| `src/main.cpp`                | Added `#include "core/tasks.h"`; removed `delayWithFace`/`pressingCheck`/`recordInput` implementations; `loop()` → `vTaskDelay(portMAX_DELAY)`; `setup()` calls `CmdQueue::init()` then `Tasks::startAll()` |
| `src/motors/poses.cpp`        | Replaced `extern void delayWithFace` + `extern bool pressingCheck` inline decls with `#include "core/tasks.h"`                                                                                              |
| `src/motors/servo_driver.cpp` | Replaced `extern void delayWithFace` inline decl with `#include "core/tasks.h"`                                                                                                                             |

**Task design:**

| Task          | Priority | Stack | Body                                                             |
| ------------- | -------- | ----- | ---------------------------------------------------------------- |
| `taskWeb`     | 1        | 8 KB  | `Web::pump()` + `vTaskDelay(1 ms)`                               |
| `taskDisplay` | 1        | 4 KB  | `Display::tickFace/Idle/Marquee()` + `vTaskDelay(10 ms)`         |
| `taskMotor`   | 2        | 8 KB  | `CmdQueue::pop()` + dispatcher + serial CLI + `vTaskDelay(1 ms)` |

**Design decisions:**

- `delayWithFace(ms)` → `vTaskDelay(pdMS_TO_TICKS(ms))`: web and display are driven
  by their own tasks, so spin-polling is no longer needed inside pose/gait routines.
- `pressingCheck(cmd, ms)` polls `CmdQueue::pop()` every 5 ms instead of checking
  `currentCommand` directly; ensures interrupt-responsiveness inside long gaits.
- `currentCommand` remains a plain global in `main.cpp` for Phase 5; reads/writes are
  still unprotected — that is addressed in Phase 6.
- `recordInput()` is inlined as `Display::notifyInput()` inside `tasks.cpp`.
- `frameDelay`, `walkCycles`, `motorCurrentDelay` stay as `extern` globals for Phase 5.
- Arduino `loop()` uses `vTaskDelay(portMAX_DELAY)` — the standard pattern for handing
  full control to FreeRTOS tasks on Arduino-ESP32.

**Verification:**

```
pio run   →  SUCCESS  RAM 17.0%  Flash 71.5%
```

**Commit:** _to be filled_

---

## Phase 6 — Thread-Safety Hardening

**Goal:** Eliminate data races between the three FreeRTOS tasks on all shared
state variables identified in PLAN.md.

**Files added / changed:**

| File                            | Change                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| ------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `include/display/face_engine.h` | Removed `extern String currentFaceName` and `extern int faceFps`; added `getCurrentFaceName()`, `getFaceFps()`, `setFaceFps(int)` thread-safe accessor declarations                                                                                                                                                                                                                                                                      |
| `src/display/face_engine.cpp`   | Added `#include <freertos/semphr.h>`; `s_currentFaceName` and `s_faceFps` made private static; mutex `s_faceNameMutex` (SemaphoreHandle_t) for String; spinlock `s_faceMux` (portMUX_TYPE) for int; mutex created in `Display::init()`; `Display::set()` reads/writes name under mutex; `getFaceFpsForName()` and `tickFace()` read faceFps under spinlock; accessors `getCurrentFaceName()`, `getFaceFps()`, `setFaceFps()` implemented |
| `src/main.cpp`                  | Added `#include <atomic>`; `frameDelay`, `walkCycles`, `motorCurrentDelay` changed from `int` to `std::atomic<int>`                                                                                                                                                                                                                                                                                                                      |
| `src/web/web_server.cpp`        | Added `#include <atomic>`; updated extern declarations to `std::atomic<int>`; replaced `Display::currentFaceName` reads with `Display::getCurrentFaceName()`; replaced `Display::faceFps` read/write with `Display::getFaceFps()` / `Display::setFaceFps()`                                                                                                                                                                              |
| `src/motors/poses.cpp`          | Updated `extern int frameDelay/walkCycles` to `extern std::atomic<int>`                                                                                                                                                                                                                                                                                                                                                                  |
| `src/motors/servo_driver.cpp`   | Updated `extern int motorCurrentDelay` to `extern std::atomic<int>`                                                                                                                                                                                                                                                                                                                                                                      |
| `src/core/tasks.cpp`            | Removed unused `extern int` declarations for timing params                                                                                                                                                                                                                                                                                                                                                                               |

**Protection strategy:**

| Shared variable                                 | Writers                                  | Readers                                          | Mechanism                                                                         |
| ----------------------------------------------- | ---------------------------------------- | ------------------------------------------------ | --------------------------------------------------------------------------------- |
| `currentCommand`                                | TaskMotor (queue pop, poses, serial CLI) | TaskWeb (status API)                             | FreeRTOS queue (Phase 4); read race accepted (String assign is fast, status-only) |
| `s_currentFaceName`                             | TaskMotor (`Display::set()`)             | TaskWeb (`handleGetStatus`, `handleTerminalCmd`) | `SemaphoreHandle_t` mutex — String heap ops require proper mutex, not spinlock    |
| `s_faceFps`                                     | TaskWeb (`handleSetSettings`)            | TaskDisplay (`tickFace` fallback)                | `portENTER_CRITICAL` spinlock — int read/write, well under 1 µs                   |
| `frameDelay`, `walkCycles`, `motorCurrentDelay` | TaskWeb (`handleSetSettings`)            | TaskMotor (poses, servo_driver)                  | `std::atomic<int>` — hardware-atomic on Xtensa 32-bit + compiler fence            |

**Design decisions:**

- `Display::set()` takes the mutex only for the short String copy at the start
  (idempotency check) and at the end (write resolved name). The heavy face
  lookup and I2C bitmap update happen outside the lock, keeping contention
  minimal.
- `portENTER_CRITICAL` is used only for bare `int` operations (< 1 µs).
  `String` operations (heap alloc) are never inside a critical section.
- `std::atomic<int>` with default `seq_cst` ordering is transparent to all
  callers — `operator int()` and `operator=(int)` make all existing read/write
  sites compile unchanged.
- `currentCommand` read in `handleGetStatus` / `handleTerminalCmd` is not
  explicitly locked — this reflects the plan's note that it is "already
  resolved via queue" for the write path. The status read is best-effort.

**Verification:**

```
pio run   →  SUCCESS  RAM 17.0%  Flash 71.6%
```

**Commit:** _to be filled_
