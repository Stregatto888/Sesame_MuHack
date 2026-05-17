# SESAME MUHACK

<div align="center">

```
 ____  _____ ____    _    __  __ _____   __  __ _   _ _   _    _    ____ _  __
/ ___|| ____/ ___|  / \  |  \/  | ____| |  \/  | | | | | | |  / \  / ___| |/ /
\___ \|  _| \___ \ / _ \ | |\/| |  _|   | |\/| | | | | |_| | / _ \| |   | ' /
 ___) | |___ ___) / ___ \| |  | | |___  | |  | | |_| |  _  |/ ___ \ |___| . \
|____/|_____|____/_/   \_\_|  |_|_____| |_|  |_|\___/|_| |_/_/   \_\____|_|\_\
```

**quadruped · networked · slightly dangerous**

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Ready-orange)](https://platformio.org)
[![ESP32-S2](https://img.shields.io/badge/ESP32-S2-blue?logo=espressif)](https://www.espressif.com/en/products/socs/esp32-s2)
[![FreeRTOS](https://img.shields.io/badge/FreeRTOS-3_tasks-brightgreen)](https://www.freertos.org)
[![Flash](https://img.shields.io/badge/Flash-29.8%25-success)](platformio.ini)
[![MuHack](https://img.shields.io/badge/Made%20by-MuHack%20Brescia-e63946)](https://muhack.org)

Fork of **[dorianborian/sesame-robot](https://github.com/dorianborian/sesame-robot)** · hacked by **[MuHack Brescia](https://muhack.org)**

</div>

---

## WHAT IT IS

8-servo ESP32-S2 quadruped robot with OLED face display, WiFi captive portal, browser terminal, and FreeRTOS multitasking. Point a phone at the AP, own the robot.

The mechanical design, movement algorithms, and face animation system are all by [Dorian Todd](https://github.com/dorianborian). MuHack took the firmware and refactored it.

---

## WHAT MUHACK ADDED

|                          |                                                                                      |
| ------------------------ | ------------------------------------------------------------------------------------ |
| **PlatformIO**           | Professional build system. No Arduino IDE. `pio run --target upload` and done.       |
| **FreeRTOS tasks**       | Three concurrent tasks: TaskWeb / TaskDisplay / TaskMotor. No more cooperative loop. |
| **Modular architecture** | 6-phase refactor: motors / display / web / core split into proper modules.           |
| **Thread-safe state**    | `std::atomic<int>` + `SemaphoreHandle_t` mutex + `portMUX_TYPE` spinlock.            |
| **Command queue**        | FreeRTOS queue decouples HTTP handlers from motor execution.                         |
| **Browser terminal**     | Web CLI — type commands directly from the captive portal.                            |
| **Hack lock**            | `hack` locks the robot to your IP. `muhack` releases it.                             |
| **Huge app partition**   | Dropped dual-OTA scheme. Flash usage: 71.6% → **29.8%**.                             |

---

## FLASH IT

```bash
git clone https://github.com/Stregatto888/Sesame_MuHack.git
cd Sesame_MuHack
pio run --target upload
pio device monitor
```

> Requires [PlatformIO Core](https://platformio.org/install/cli) and a USB cable.  
> External 5V ≥ 2A power supply needed for the servos.

---

## CONNECT

1. Join WiFi: **`Sesame MuHack`** · password: `12345678`
2. Browser auto-opens captive portal — or navigate to `http://192.168.4.1`
3. _(Optional)_ Connect robot to your LAN: set `NETWORK_SSID` / `NETWORK_PASS` in [include/core/config.h](include/core/config.h) → access at `http://sesame-robot.local`

---

## HTTP API

| Method | Route                         | What it does                                                     |
| ------ | ----------------------------- | ---------------------------------------------------------------- |
| `GET`  | `/cmd?go=forward`             | Walk forward (continuous until stopped)                          |
| `GET`  | `/cmd?go=backward`            | Walk backward                                                    |
| `GET`  | `/cmd?go=left`                | Turn left                                                        |
| `GET`  | `/cmd?go=right`               | Turn right                                                       |
| `GET`  | `/cmd?pose=dance`             | Execute a one-shot pose                                          |
| `GET`  | `/cmd?stop=1`                 | Stop all motion                                                  |
| `GET`  | `/cmd?motor=R1&value=135`     | Drive one servo directly                                         |
| `POST` | `/api/command`                | JSON body: `{"command":"wave","face":"happy"}`                   |
| `GET`  | `/api/status`                 | JSON: current command, face, IP, lock state                      |
| `GET`  | `/terminal?cmd=status`        | Browser terminal endpoint                                        |
| `GET`  | `/getSettings`                | JSON: `frameDelay`, `walkCycles`, `motorCurrentDelay`, `faceFps` |
| `GET`  | `/setSettings?frameDelay=150` | Update runtime params                                            |

**Examples:**

```bash
# Walk forward
curl "http://192.168.4.1/cmd?go=forward"

# Dance with a happy face
curl -X POST http://192.168.4.1/api/command \
  -H "Content-Type: application/json" \
  -d '{"command":"dance","face":"happy"}'

# Get robot status
curl http://192.168.4.1/api/status
```

---

## TERMINAL COMMANDS

Open the web UI → terminal tab, or `GET /terminal?cmd=<command>`:

| Command                                                             | Effect                                                              |
| ------------------------------------------------------------------- | ------------------------------------------------------------------- |
| `forward` / `backward` / `left` / `right`                           | Move                                                                |
| `stop`                                                              | Stop all motion                                                     |
| `rest` `stand` `wave` `dance` `swim` `point`                        | Poses                                                               |
| `pushup` `bow` `cute` `freaky` `worm` `shake` `shrug` `dead` `crab` | More poses                                                          |
| `status`                                                            | Show IP, SSID, connected clients, current command, face, lock state |
| `help`                                                              | List all commands                                                   |
| `hack`                                                              | Lock robot to your IP — other clients get ACCESS DENIED             |
| `muhack`                                                            | Release lock (owner IP only)                                        |

---

## FACES

37 face animations across three categories:

```
── MOVEMENT ─────────────────────────────────────────────────────────
  walk  rest  swim  dance  wave  point  stand

── POSES ────────────────────────────────────────────────────────────
  cute  pushup  freaky  bow  worm  shake  shrug  dead  crab

── IDLE ─────────────────────────────────────────────────────────────
  idle  idle_blink  defualt

── EMOTIONS (+ talking variants for voice assistant integration) ─────
  happy      talk_happy    sad        talk_sad      angry      talk_angry
  surprised  talk_surprised  sleepy   talk_sleepy   love       talk_love
  excited    talk_excited  confused   talk_confused  thinking  talk_thinking
```

Set a face independently of movement:

```bash
curl -X POST http://192.168.4.1/api/command \
  -d '{"face":"thinking"}'
```

---

## HARDWARE

| Component    | Detail                                               |
| ------------ | ---------------------------------------------------- |
| MCU          | Lolin S2 Mini (ESP32-S2, 240 MHz, single core)       |
| Display      | SSD1306 128×64 OLED · I²C SDA=GPIO 35, SCL=GPIO 33   |
| Servos       | 8× MG90 · GPIO {1, 2, 4, 6, 8, 10, 13, 14}           |
| Leg topology | R1/R2/L1/L2 = hip servos · R3/R4/L3/L4 = foot servos |
| Power        | 5V ≥ 2A external (USB-C PD or bench supply)          |
| Flash        | 4 MB · huge_app partition · 938 KB used (29.8%)      |
| RAM          | 320 KB · 56 KB used (17.0%)                          |

---

## ARCHITECTURE

### System Overview

```mermaid
graph TB
    subgraph HW["HARDWARE"]
        OLED["SSD1306 OLED\n128×64 · I²C 0x3C"]
        SERVOS["8× MG90 Servos\nGPIO 1,2,4,6,8,10,13,14"]
        WIFI["ESP32-S2 WiFi\nAP: 192.168.4.1"]
    end

    subgraph CLIENTS["CLIENTS"]
        BROWSER["Browser / Phone\nCaptive Portal"]
        CURL["curl / Python\nHTTP API"]
        SERIAL["Serial Terminal\n115200 baud"]
    end

    subgraph RTOS["FreeRTOS — ESP32-S2 single core"]
        direction TB

        subgraph TW["TaskWeb · priority 1 · stack 8KB"]
            DNS["DNSServer\nwildcard *.→ 192.168.4.1"]
            HTTP["WebServer :80\n8 routes"]
            LOCK["Hack-lock\nstate machine"]
        end

        subgraph TD["TaskDisplay · priority 1 · stack 4KB"]
            TICK["tickFace()\nframe advance"]
            IDLE["tickIdle()\nblink automaton"]
            MARQ["tickMarquee()\nWiFi info scroll"]
        end

        CQ["CmdQueue\nFreeRTOS queue\ndepth 4 · 32 B/item"]

        subgraph TM["TaskMotor · priority 2 · stack 8KB"]
            DISP["Command\nDispatcher"]
            POSES["Pose / Gait\nFunctions"]
            CLI["Serial CLI\nparser"]
        end
    end

    subgraph SHARED["SHARED STATE — thread-safe"]
        FN["s_currentFaceName\nSemaphoreHandle_t mutex"]
        FFPS["s_faceFps\nportMUX_TYPE spinlock"]
        ATOM["frameDelay · walkCycles\nmotorCurrentDelay\nstd::atomic&lt;int&gt;"]
    end

    BROWSER -->|HTTP GET/POST| HTTP
    CURL -->|HTTP GET/POST| HTTP
    SERIAL -->|UART| CLI

    DNS -->|resolves to AP IP| WIFI
    HTTP -->|CmdQueue::push| CQ
    CQ -->|CmdQueue::pop| DISP
    DISP --> POSES
    POSES -->|Motors::setAngle| SERVOS
    POSES -->|Display::set| FN

    HTTP -->|read| FN
    HTTP -->|read/write| FFPS
    HTTP -->|read/write| ATOM
    TM -->|read| ATOM

    TICK -->|drawBitmap| OLED
    TICK -->|read| FN
    TICK -->|read| FFPS
    IDLE -->|tickBlink| OLED
    MARQ -->|scrollText| OLED

    style TW fill:#1a1a2e,color:#00d4ff
    style TD fill:#1a1a2e,color:#00d4ff
    style TM fill:#1a1a2e,color:#e63946
    style CQ fill:#2a2a3e,color:#ffd700
    style SHARED fill:#0d1117,color:#3fb950
```

---

### TaskWeb — detail

```mermaid
flowchart TD
    START([loop start]) --> PUMP["Web::pump()"]

    PUMP --> DNS_PROC["dnsServer.processNextRequest()\n— returns immediately if no query —"]
    DNS_PROC --> HTTP_PROC["server.handleClient()\n— dispatches one pending request —"]
    HTTP_PROC --> ROUTE{route?}

    ROUTE -->|GET /| ROOT["handleRoot()\nserve index_html PROGMEM"]
    ROUTE -->|GET /cmd| CMD["handleCommandWeb()\npose= / go= / stop= / motor="]
    ROUTE -->|GET /terminal| TERM["handleTerminalCmd()\ncmd= parameter"]
    ROUTE -->|POST /api/command| APICMD["handleApiCommand()\nJSON body parse"]
    ROUTE -->|GET /api/status| STATUS["handleGetStatus()\nJSON: command+face+IP"]
    ROUTE -->|GET /getSettings| GETS["handleGetSettings()\nJSON: 4 timing params"]
    ROUTE -->|GET /setSettings| SETS["handleSetSettings()\nwrite atomic params"]
    ROUTE -->|*| ROOT

    TERM --> HACK_CHECK{cmd?}
    HACK_CHECK -->|hack| LOCK["hackLocked=true\nhackOwnerIP=clientIP\n→ 200 ACCESS GRANTED"]
    HACK_CHECK -->|muhack| UNLOCK{"owner IP?"}
    UNLOCK -->|yes| FREE["hackLocked=false\n→ 200 LOCK RELEASED"]
    UNLOCK -->|no| DENY2["→ 200 ERROR: only hacker"]
    HACK_CHECK -->|status| STAT_RESP["IP+SSID+clients\n+command+face+lockState"]
    HACK_CHECK -->|movement/pose| BLOCKED{"isHackBlocked()?"}
    BLOCKED -->|yes| DENY["→ 200 ACCESS DENIED"]
    BLOCKED -->|no| ENQUEUE["CmdQueue::push(cmd)\nDisplay::notifyInput()\nDisplay::exitIdle()"]

    APICMD --> PARSE["indexOf 'command'\nindexOf 'face'"]
    PARSE -->|face only| FACE_SET["Display::set(face)\nDisplay::notifyInput()"]
    PARSE -->|command| CMD_PUSH["CmdQueue::push(cmd)\noptional Display::set(face)"]

    SETS --> WRITE["frameDelay.store()\nwalkCycles.store()\nmotorCurrentDelay.store()\nDisplay::setFaceFps()"]

    ENQUEUE --> DELAY["vTaskDelay(1ms)"]
    ROOT --> DELAY
    DELAY --> START

    style LOCK fill:#e63946,color:#fff
    style FREE fill:#3fb950,color:#000
    style DENY fill:#e63946,color:#fff
    style DENY2 fill:#e63946,color:#fff
    style ENQUEUE fill:#2a2a3e,color:#ffd700
    style CMD_PUSH fill:#2a2a3e,color:#ffd700
```

---

### TaskDisplay — detail

```mermaid
flowchart TD
    START([loop start]) --> TF["Display::tickFace()"]

    TF --> CHKFRAMES{"currentFaceFrames\n!= nullptr?"}
    CHKFRAMES -->|no| TI
    CHKFRAMES -->|yes| ELAPSED{"millis() - lastFaceFrameMs\n≥ 1000/currentFaceFps?"}
    ELAPSED -->|no| TI
    ELAPSED -->|yes| MODE{currentFaceMode?}

    MODE -->|LOOP| LOOP_ADV["frameIndex = (frameIndex+1)\n% frameCount"]
    MODE -->|ONCE| ONCE_CHECK{"frameIndex\n< frameCount-1?"}
    ONCE_CHECK -->|yes| ONCE_ADV["frameIndex++"]
    ONCE_CHECK -->|no| ONCE_FREEZE["faceAnimFinished = true\n(hold last frame)"]
    MODE -->|BOOMERANG| BOOM_ADV["frameIndex += direction\nif hit end/start: flip direction"]

    LOOP_ADV --> DRAW
    ONCE_ADV --> DRAW
    ONCE_FREEZE --> DRAW
    BOOM_ADV --> DRAW

    DRAW["updateFaceBitmap(frames[frameIndex])\ndisplay.clearDisplay()\ndisplay.drawBitmap(128×64)\ndisplay.display()"] --> TI

    TI["Display::tickIdle()"] --> IDLE_CHK{"idleActive?"}
    IDLE_CHK -->|no| TM
    IDLE_CHK -->|yes| BLINK_CHK{"idleBlinkActive?"}

    BLINK_CHK -->|no| SCHED_CHK{"millis() ≥\nnextIdleBlinkMs?"}
    SCHED_CHK -->|no| TM
    SCHED_CHK -->|yes| DBLCHK{"30% chance\ndouble blink?"}
    DBLCHK -->|yes| DOUBLE["idleBlinkRepeatsLeft = 2"]
    DBLCHK -->|no| SINGLE["idleBlinkRepeatsLeft = 1"]
    DOUBLE --> START_BLINK
    SINGLE --> START_BLINK["Display::set('idle_blink')\nFACE_ANIM_ONCE\nidleBlinkActive = true"]

    BLINK_CHK -->|yes| FIN_CHK{"faceAnimFinished?"}
    FIN_CHK -->|no| TM
    FIN_CHK -->|yes| REPEAT_CHK{"idleBlinkRepeatsLeft > 1?"}
    REPEAT_CHK -->|yes| START_BLINK2["idleBlinkRepeatsLeft--\nrestart idle_blink ONCE"]
    REPEAT_CHK -->|no| RETURN_IDLE["Display::set('idle')\nFACE_ANIM_BOOMERANG\nidleBlinkActive = false\nscheduleNextBlink(3-7s)"]

    START_BLINK --> TM
    START_BLINK2 --> TM
    RETURN_IDLE --> TM

    TM["Display::tickMarquee()"] --> WIFI_CHK{"firstInputReceived\nOR !showingWifiInfo?"}
    WIFI_CHK -->|input received| HIDE["showingWifiInfo = false"]
    WIFI_CHK -->|no input, 30s elapsed| SHOW_CHK{"showingWifiInfo?"}
    SHOW_CHK -->|no| ACTIVATE["showingWifiInfo = true\nwifiScrollPos = 0"]
    SHOW_CHK -->|yes| SCROLL_CHK{"millis() - lastScrollMs\n≥ 150ms?"}
    SCROLL_CHK -->|yes| SCROLL["draw black bar top 10px\nrenderText(wifiInfoText, -scrollPos)\nscrollPos += 2\nwrap at text length × 6"]

    HIDE --> DELAY
    ACTIVATE --> DELAY
    SCROLL --> DELAY["vTaskDelay(10ms)"]
    DELAY --> START

    style DRAW fill:#1a1a2e,color:#00d4ff
    style START_BLINK fill:#2a2a3e,color:#ffd700
    style RETURN_IDLE fill:#2a2a3e,color:#ffd700
    style SCROLL fill:#1a1a2e,color:#3fb950
```

---

### TaskMotor — detail

```mermaid
flowchart TD
    START([loop start]) --> POP["CmdQueue::pop(incoming)"]
    POP -->|got command| SET_CMD["currentCommand = incoming"]
    POP -->|queue empty| DISPATCH

    SET_CMD --> DISPATCH{"currentCommand\n!= empty?"}

    DISPATCH -->|no| SERIAL_CHK
    DISPATCH -->|yes| CMD_SW{command?}

    CMD_SW -->|forward| WALK["runWalkPose()\nloop walkCycles × gait\ninterruptible via pressingCheck()"]
    CMD_SW -->|backward| WALKB["runWalkBackward()"]
    CMD_SW -->|left| TURNL["runTurnLeft()"]
    CMD_SW -->|right| TURNR["runTurnRight()"]
    CMD_SW -->|rest| REST["runRestPose()\nall servos→90°\ncurrentCommand=''"]
    CMD_SW -->|stand| STAND["runStandPose(1)\npose + enterIdle()\ncurrentCommand=''"]
    CMD_SW -->|wave dance swim point\npushup bow cute freaky\nworm shake shrug dead crab| POSE["run<Pose>Pose()\nDisplay::set(face)\ncurrentCommand=''"]

    WALK --> PRESS_CHK["pressingCheck(cmd, frameDelay)\npoll CmdQueue every 5ms\nabort → runStandPose(1)"]
    WALKB --> PRESS_CHK
    TURNL --> PRESS_CHK
    TURNR --> PRESS_CHK
    PRESS_CHK -->|new cmd arrived| SET_CMD
    PRESS_CHK -->|timeout OK| DISPATCH

    POSE --> SERVOS["Motors::setAngle(ch, angle)\n  + servoSubtrim[ch]\n  + delayWithFace(motorCurrentDelay)\nfor each keyframe"]
    SERVOS --> DISPATCH

    REST --> DISPATCH
    STAND --> DISPATCH

    DISPATCH --> SERIAL_CHK{"Serial.available()?"}
    SERIAL_CHK -->|no| DELAY
    SERIAL_CHK -->|yes| READ_CHAR["read char → buffer\n(32 B static ring)"]
    READ_CHAR --> NEWLINE{"\\n or \\r?"}
    NEWLINE -->|no| SERIAL_CHK
    NEWLINE -->|yes| CLI_PARSE{parse command}

    CLI_PARSE -->|"rn wf/wb/tl/tr"| MOV_CMD["currentCommand = cmd\nrun<Walk/Turn>Pose()"]
    CLI_PARSE -->|"rn rs/st/wv/dn/..."| POSE_CMD["run<Pose>Pose()"]
    CLI_PARSE -->|"N deg"| ONE_SERVO["Motors::setAngle(N, deg)"]
    CLI_PARSE -->|"all deg"| ALL_SERVO["for i in 0..7:\nMotors::setAngle(i, deg)"]
    CLI_PARSE -->|"st N V"| SUBTRIM["Motors::subtrim[N] = V"]
    CLI_PARSE -->|"st save"| ST_SAVE["print C array\nto Serial"]
    CLI_PARSE -->|"st reset"| ST_RESET["memset subtrim[], 0, 8"]

    MOV_CMD --> CLEAR["buffer_pos = 0"]
    POSE_CMD --> CLEAR
    ONE_SERVO --> CLEAR
    ALL_SERVO --> CLEAR
    SUBTRIM --> CLEAR
    ST_SAVE --> CLEAR
    ST_RESET --> CLEAR
    CLEAR --> DELAY["vTaskDelay(1ms)"]
    DELAY --> START

    style WALK fill:#e63946,color:#fff
    style WALKB fill:#e63946,color:#fff
    style TURNL fill:#e63946,color:#fff
    style TURNR fill:#e63946,color:#fff
    style POSE fill:#2a2a3e,color:#ffd700
    style PRESS_CHK fill:#1a1a2e,color:#00d4ff
    style SERVOS fill:#1a1a2e,color:#3fb950
```

**Thread-safety contracts:**

| Shared variable                                 | Mechanism                 |
| ----------------------------------------------- | ------------------------- |
| Face name (`String`)                            | `SemaphoreHandle_t` mutex |
| Face FPS (`int`)                                | `portMUX_TYPE` spinlock   |
| `frameDelay`, `walkCycles`, `motorCurrentDelay` | `std::atomic<int>`        |

---

## SERIAL CLI

115200 baud. Connect with `pio device monitor`.

| Command                                                 | Effect                                             |
| ------------------------------------------------------- | -------------------------------------------------- |
| `rn wf` / `rn wb` / `rn tl` / `rn tr`                   | Walk fwd/bwd/left/right                            |
| `rn rs` / `rn st`                                       | Rest / stand                                       |
| `rn wv` `rn dn` `rn sw` `rn pt` `rn pu` `rn bw`         | Wave / dance / swim / point / pushup / bow         |
| `rn ct` `rn fk` `rn wm` `rn sk` `rn sg` `rn dd` `rn cb` | Cute / freaky / worm / shake / shrug / dead / crab |
| `<n> <deg>`                                             | Set servo n (0–7) to angle                         |
| `all <deg>`                                             | Set all 8 servos to same angle                     |
| `st <n> <v>`                                            | Set subtrim offset for servo n                     |
| `st save`                                               | Print subtrim C array to paste into source         |
| `st reset`                                              | Zero all subtrims                                  |

---

## CREDITS

**Original project — [Sesame Robot by Dorian Todd](https://github.com/dorianborian/sesame-robot)**  
All core movement algorithms, servo kinematics, face animation system, and hardware design are Dorian's work. This fork builds on his foundation — go star the original.

**MuHack Brescia Edition** — [muhack.org](https://muhack.org)  
PlatformIO migration, FreeRTOS refactor (6 phases), web terminal, hack lock, MuHack branding.

**Libraries**  
[ESP32Servo](https://github.com/madhephaestus/ESP32Servo) by Kevin Harrington ·
[Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306) ·
[Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) ·
ESP32 Arduino Core by Espressif Systems

---

<div align="center">

**Made with ❤️ and ⚡ at [MuHack Brescia](https://muhack.org)**

_Issues and PRs welcome._

</div>
