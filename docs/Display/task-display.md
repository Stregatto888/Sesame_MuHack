# TaskDisplay — Detail

**Priority:** 1 · **Stack:** 4 KB · **Loop period:** 10 ms

Owns the OLED. Runs three independent tick functions each loop: face frame advance, idle-blink state machine, and WiFi info marquee scroll.

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

## Animation modes

| Mode                  | Behaviour                                                      | Typical use                    |
| --------------------- | -------------------------------------------------------------- | ------------------------------ |
| `FACE_ANIM_LOOP`      | Cycles frames 0→N→0→N forever                                  | Continuous gaits (walk, dance) |
| `FACE_ANIM_ONCE`      | Plays 0→N, freezes on last frame, sets `faceAnimFinished=true` | One-shot poses, idle_blink     |
| `FACE_ANIM_BOOMERANG` | Ping-pong 0→N→0, direction flips at boundaries                 | Rest, idle, point              |

## Idle-blink timing

- **Blink interval:** random 3–7 s
- **Double-blink probability:** 30%
- **Frame rate of idle_blink:** 7 FPS (from per-face FPS table)
- **Return to idle:** immediately after `faceAnimFinished` is set

## Related diagrams

- [System overview](../Architecture/architecture-overview.md)
- [TaskWeb detail](../Web/task-web.md)
- [TaskMotor detail](../Motor/task-motor.md)
