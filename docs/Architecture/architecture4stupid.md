# Architecture — How It Works

All three FreeRTOS tasks, the command queue, the physical hardware, the user interfaces, and the shared data with thread protection.

```mermaid
graph TB
subgraph HW["PHYSICAL HARDWARE"]
OLED["OLED Screen\n(The robot's face)"]
SERVOS["8 Servo Motors\n(The robot's legs)"]
WIFI["WiFi Module\n(Creates the robot's network)"]
end

    subgraph CLIENTS["USER INTERFACES"]
        BROWSER["Smartphone / PC\n(Web App)"]
        CURL["Script / API\n(External integrations)"]
        SERIAL["Serial Terminal\n(USB cable)"]
    end

    subgraph RTOS["MULTITASKING SYSTEM (FreeRTOS)"]
        direction TB

        subgraph TW["1. Network Brain (TaskWeb)"]
            DNS["DNS Trap\n(Forces the Web App to open)"]
            HTTP["Web Server\n(Handles user requests)"]
            LOCK["Security Manager\n(Locks / Unlocks control)"]
        end

        subgraph TD["2. Visual Brain (TaskDisplay)"]
            TICK["Face Animator\n(Advances frames)"]
            IDLE["Idle Manager\n(Blinks the eyes)"]
            MARQ["Banner Manager\n(Shows WiFi info when idle)"]
        end

        CQ["Command Queue\n(Decouples Network from Motors)"]

        subgraph TM["3. Motor Brain (TaskMotor)"]
            DISP["Command Dispatcher\n(Decides what to do)"]
            POSES["Movement Library\n(Poses and Gaits)"]
            CLI["Diagnostic Terminal\n(Listens on USB cable)"]
        end
    end

    subgraph SHARED["SHARED DATA (Safe for concurrent access)"]
        FN["Current Face Name\n(Protected against mid-write reads)"]
        FFPS["Animation Speed\n(Protected against simultaneous writes)"]
        ATOM["Motion Parameters\n(Speed, Cycles, Delays)"]
    end

    BROWSER -->|Web requests| HTTP
    CURL -->|Web requests| HTTP
    SERIAL -->|Text| CLI

    DNS -->|Redirects to| WIFI
    HTTP -->|Sends command| CQ
    CQ -->|Reads command| DISP
    DISP --> POSES
    POSES -->|Physically moves| SERVOS
    POSES -->|Changes expression| FN

    HTTP -->|Reads for status display| FN
    HTTP -->|Reads / writes| FFPS
    HTTP -->|Reads / writes| ATOM
    TM -->|Reads to move| ATOM

    TICK -->|Draws pixels| OLED
    TICK -->|Reads expression| FN
    TICK -->|Reads speed| FFPS
    IDLE -->|Forces eye blink| OLED
    MARQ -->|Scrolls text| OLED

    style TW fill:#1a1a2e,color:#00d4ff
    style TD fill:#1a1a2e,color:#00d4ff
    style TM fill:#1a1a2e,color:#e63946
    style CQ fill:#2a2a3e,color:#ffd700
    style SHARED fill:#0d1117,color:#3fb950
```

## Shared data safety

Three tasks run in parallel and read/write the same data.
To avoid corruption, each shared variable is protected by a different mechanism:

| Shared variable | Who writes | Who reads | Protection used |
| --- | --- | --- | --- |
| Current face name (`String`) | TaskMotor (`Display::set`) | TaskWeb (status/terminal) | Mutex (`SemaphoreHandle_t`) |
| Animation speed (`int`) | TaskWeb (`setSettings`) | TaskDisplay (`tickFace`) | Spinlock (`portMUX_TYPE`) |
| `frameDelay`, `walkCycles`, `motorCurrentDelay` (`int`) | TaskWeb (`setSettings`) | TaskMotor (poses, servo) | Atomic variable (`std::atomic<int>`) |
| Command queue (`CmdQueue`) | TaskWeb (HTTP handlers) | TaskMotor (dispatcher) | FreeRTOS queue (interrupt-safe) |

## Related diagrams

- [TaskWeb — How It Works](../Web/web4stupid.md)
- [TaskDisplay — How It Works](../Display/display4stupid.md)
- [TaskMotor — How It Works](../Motor/motor4stupid.md)
