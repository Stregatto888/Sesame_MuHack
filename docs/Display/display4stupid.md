# TaskDisplay — How It Works

**Priority:** 1 · **Stack:** 4 KB · **Loop period:** every 10 ms

Controls the OLED screen. Each loop it runs three functions in sequence: advance the face animation frames, handle the automatic eye blink, and scroll the WiFi banner when the robot is idle.

```mermaid
flowchart TD
START([Display Loop Start]) --> TF["Step 1: Face Animation\n(Advance frames)"]

    TF --> CHKFRAMES{"Does the requested face\nhave any frames?"}
    CHKFRAMES -->|No| TI
    CHKFRAMES -->|Yes| ELAPSED{"Has enough time passed\nfor the next frame?\n(Based on FPS)"}
    ELAPSED -->|No, wait| TI
    ELAPSED -->|Yes| MODE{Playback\nMode?}

    MODE -->|Infinite Loop| LOOP_ADV["Advance to next.\nIf at end, restart from beginning."]
    MODE -->|Play Once| ONCE_CHECK{"Am I on the last frame?"}
    ONCE_CHECK -->|No| ONCE_ADV["Advance to next frame"]
    ONCE_CHECK -->|Yes| ONCE_FREEZE["Mark animation finished\n(Hold on last frame)"]
    MODE -->|Back and Forth| BOOM_ADV["Advance. If end reached,\nstart going backward."]

    LOOP_ADV --> DRAW
    ONCE_ADV --> DRAW
    ONCE_FREEZE --> DRAW
    BOOM_ADV --> DRAW

    DRAW["Update OLED Screen\n(Draw new 128x64 pixels)"] --> TI

    TI["Step 2: Idle Management\n(Makes the robot feel alive)"] --> IDLE_CHK{"Is the robot standing\nstill (idle)?"}
    IDLE_CHK -->|No| TM
    IDLE_CHK -->|Yes| BLINK_CHK{"Is it already\nblinking?"}

    BLINK_CHK -->|No| SCHED_CHK{"Is it time for a\nrandom blink?"}
    SCHED_CHK -->|No| TM
    SCHED_CHK -->|Yes| DBLCHK{"30% chance\nof a double blink"}
    DBLCHK -->|Yes| DOUBLE["Set 2 blinks"]
    DBLCHK -->|No| SINGLE["Set 1 blink"]
    DOUBLE --> START_BLINK
    SINGLE --> START_BLINK["Force 'blink' expression\n(Played once only)"]

    BLINK_CHK -->|Yes| FIN_CHK{"Has the blink\nfinished?"}
    FIN_CHK -->|No| TM
    FIN_CHK -->|Yes| REPEAT_CHK{"Was a double blink\nscheduled?"}
    REPEAT_CHK -->|Yes| START_BLINK2["Repeat the blink"]
    REPEAT_CHK -->|No| RETURN_IDLE["Return to normal face\nRandomly decide when\nto blink next (3-7s)"]

    START_BLINK --> TM
    START_BLINK2 --> TM
    RETURN_IDLE --> TM

    TM["Step 3: Banner Management\n(Shows WiFi info)"] --> WIFI_CHK{"Was a button pressed\nrecently?"}
    WIFI_CHK -->|Yes, user active| HIDE["Hide WiFi text"]
    WIFI_CHK -->|No, idle for 30s| SHOW_CHK{"Is the banner\nalready visible?"}
    SHOW_CHK -->|No| ACTIVATE["Show it and\nstart scrolling from the right"]
    SHOW_CHK -->|Yes| SCROLL_CHK{"Is it time to move\nthe text one pixel?"}
    SCROLL_CHK -->|Yes| SCROLL["Draw black rectangle\nWrite text shifted left\nIf end reached, restart"]

    HIDE --> DELAY
    ACTIVATE --> DELAY
    SCROLL --> DELAY["Short rest (10ms)"]
    DELAY --> START

    style DRAW fill:#1a1a2e,color:#00d4ff
    style START_BLINK fill:#2a2a3e,color:#ffd700
    style RETURN_IDLE fill:#2a2a3e,color:#ffd700
    style SCROLL fill:#1a1a2e,color:#3fb950
```

## Animation playback modes

| Mode | Behaviour | When it's used |
| --- | --- | --- |
| **Infinite Loop** (`LOOP`) | Cycles frames 0→N→0→N forever | Continuous gaits, dances |
| **Play Once** (`ONCE`) | Plays 0→N, freezes on last frame, marks "finished" | Eye blink, single poses |
| **Back and Forth** (`BOOMERANG`) | Ping-pongs 0→N→0, reverses at boundaries | Rest, idle, point |

## Eye blink timing

- **Interval between blinks:** random, between 3 and 7 seconds
- **Double-blink probability:** 30%
- **Blink animation FPS:** 7 frames per second
- **Return to normal face:** immediately after animation marks "finished"

## Related diagrams

- [System Overview](../Architecture/architecture4stupid.md)
- [TaskWeb — How It Works](../Web/web4stupid.md)
- [TaskMotor — How It Works](../Motor/motor4stupid.md)
