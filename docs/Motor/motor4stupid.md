# TaskMotor — How It Works

**Priority:** 2 (the highest) · **Stack:** 8 KB · **Loop period:** every ~1 ms

The most important task. Picks commands from the queue and executes them: walks, poses, and direct movements via USB cable. The highest priority ensures motors respond immediately.

```mermaid
flowchart TD
    START([Motor Loop Start]) --> POP["Check the\nIncoming Command Box"]
    POP -->|New command arrived| SET_CMD["Store it as\n'Current Command'"]
    POP -->|Box empty| DISPATCH

    SET_CMD --> DISPATCH{"Is there a Current Command\nto execute?"}

    DISPATCH -->|No, I'm idle| SERIAL_CHK
    DISPATCH -->|Yes| CMD_SW{What is the command?}

    CMD_SW -->|Forward| WALK["Walk\n(Repeat step cycle, stop\nif a new command arrives)"]
    CMD_SW -->|Backward| WALKB["Walk backward"]
    CMD_SW -->|Left| TURNL["Turn left"]
    CMD_SW -->|Right| TURNR["Turn right"]
    CMD_SW -->|Rest| REST["Lie down\nRelax motors\nClear current command"]
    CMD_SW -->|Stand| STAND["Stand up\nActivate idle animation\nClear current command"]
    CMD_SW -->|Wave, Dance, etc.| POSE["Execute specific pose\nChange expression\nClear current command"]

    WALK --> PRESS_CHK["Quick check:\nDid a new command arrive\nwhile I was walking?"]
    WALKB --> PRESS_CHK
    TURNL --> PRESS_CHK
    TURNR --> PRESS_CHK
    PRESS_CHK -->|Yes, new command| SET_CMD
    PRESS_CHK -->|No, keep going| DISPATCH

    POSE --> SERVOS["Apply angles to motors\n(Add calibration offset)\nWait before next step"]
    SERVOS --> DISPATCH

    REST --> DISPATCH
    STAND --> DISPATCH

    DISPATCH --> SERIAL_CHK{"Is a technician connected\nvia USB cable?"}
    SERIAL_CHK -->|No| DELAY
    SERIAL_CHK -->|Yes| READ_CHAR["Read what the technician\nis typing in the terminal"]
    READ_CHAR --> NEWLINE{"Did they press Enter?"}
    NEWLINE -->|No| SERIAL_CHK
    NEWLINE -->|Yes| CLI_PARSE{Parse the USB command}

    CLI_PARSE -->|Walk command| MOV_CMD["Force walk command"]
    CLI_PARSE -->|Pose command| POSE_CMD["Execute pose"]
    CLI_PARSE -->|Calibrate 1 motor| ONE_SERVO["Move a single motor"]
    CLI_PARSE -->|Calibrate all| ALL_SERVO["Move all motors"]
    CLI_PARSE -->|Set correction| SUBTRIM["Save calibration offset"]
    CLI_PARSE -->|Save calibration| ST_SAVE["Print code to paste into source"]
    CLI_PARSE -->|Reset calibration| ST_RESET["Zero all offsets"]

    MOV_CMD --> CLEAR["Clear USB buffer"]
    POSE_CMD --> CLEAR
    ONE_SERVO --> CLEAR
    ALL_SERVO --> CLEAR
    SUBTRIM --> CLEAR
    ST_SAVE --> CLEAR
    ST_RESET --> CLEAR
    CLEAR --> DELAY["Very short rest (1ms)\nto avoid blocking the CPU"]
    DELAY --> START

    style WALK fill:#e63946,color:#fff
    style WALKB fill:#e63946,color:#fff
    style TURNL fill:#e63946,color:#fff
    style TURNR fill:#e63946,color:#fff
    style POSE fill:#2a2a3e,color:#ffd700
    style PRESS_CHK fill:#1a1a2e,color:#00d4ff
    style SERVOS fill:#1a1a2e,color:#3fb950
```

## Interrupting a walk

Walk commands (forward/backward/left/right) can be interrupted at any time. Every 5 ms during the step cycle, `pressingCheck()` checks if a new command has arrived. If so:

1. The new command becomes the current one
2. The robot returns to a safe position (`runStandPose`)
3. The walk loop stops immediately

## How a servo is moved

```
requested_angle
  + servoSubtrim[channel]         ← calibration offset int8_t [-90, +90]
  = constrain(result, 0, 180)
  → servos[channel].write()
  → vTaskDelay(motorCurrentDelay) ← delay to spread inrush current
```

## Continuous vs. one-shot commands

| Type | Commands | Behaviour |
| --- | --- | --- |
| **Continuous** | forward, backward, left, right | Command stays active and re-dispatches every loop until interrupted |
| **One-shot** | rest, stand, wave, dance, etc. | Function clears the current command on completion |

## Related diagrams

- [System Overview](../Architecture/architecture4stupid.md)
- [TaskWeb — How It Works](../Web/web4stupid.md)
- [TaskDisplay — How It Works](../Display/display4stupid.md)
