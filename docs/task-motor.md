# TaskMotor — Detail

**Priority:** 2 · **Stack:** 8 KB · **Loop period:** ~1 ms

Highest-priority task. Pops commands from CmdQueue and dispatches them to pose/gait functions. Also owns the Serial CLI parser for direct hardware access.

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

## pressingCheck() — interruptible gait

Locomotion commands (forward/backward/left/right) are interruptible. During every inter-frame pause, `pressingCheck()` polls CmdQueue every 5 ms. If a new command arrives:

1. `currentCommand = newCmd`
2. `runStandPose(1)` — safe park position
3. Returns `false` — caller aborts the gait loop immediately

## Motors::setAngle() — per-channel pipeline

```
angle_requested
  + servoSubtrim[channel]         ← int8_t offset [-90, +90]
  = constrain(result, 0, 180)
  → servos[channel].write()
  → vTaskDelay(motorCurrentDelay) ← spread inrush current
```

## Continuous vs one-shot commands

| Type           | Commands                         | Behaviour                                                              |
| -------------- | -------------------------------- | ---------------------------------------------------------------------- |
| **Continuous** | forward, backward, left, right   | `currentCommand` stays set; re-dispatches every loop until interrupted |
| **One-shot**   | all poses (rest, stand, wave, …) | Function clears `currentCommand` itself on completion                  |

## Related diagrams

- [System overview](architecture-overview.md)
- [TaskWeb detail](task-web.md)
- [TaskDisplay detail](task-display.md)
