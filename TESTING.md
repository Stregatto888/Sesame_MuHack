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

_(To be filled after Phase 2 implementation)_

---

## Phase 3 — Extract Web Module

_(To be filled after Phase 3 implementation)_

---

## Phase 4 — FreeRTOS Queue Plumbing

_(To be filled after Phase 4 implementation)_

---

## Phase 5 — Split into 3 FreeRTOS Tasks

_(To be filled after Phase 5 implementation)_

---

## Phase 6 — Thread-Safety Hardening

_(To be filled after Phase 6 implementation)_
