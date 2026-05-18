# Architecture — System Overview

All three FreeRTOS tasks, the command queue, hardware peripherals, client interfaces, and shared thread-safe state.

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

## Thread-safety contracts

| Shared variable                                       | Writers                    | Readers                   | Mechanism                 |
| ----------------------------------------------------- | -------------------------- | ------------------------- | ------------------------- |
| `s_currentFaceName` (String)                          | TaskMotor (`Display::set`) | TaskWeb (status/terminal) | `SemaphoreHandle_t` mutex |
| `s_faceFps` (int)                                     | TaskWeb (`setSettings`)    | TaskDisplay (`tickFace`)  | `portMUX_TYPE` spinlock   |
| `frameDelay`, `walkCycles`, `motorCurrentDelay` (int) | TaskWeb (`setSettings`)    | TaskMotor (poses, servo)  | `std::atomic<int>`        |
| `CmdQueue`                                            | TaskWeb (HTTP handlers)    | TaskMotor (dispatcher)    | FreeRTOS queue (ISR-safe) |

## Related diagrams

- [TaskWeb detail](../Web/task-web.md)
- [TaskDisplay detail](../Display/task-display.md)
- [TaskMotor detail](../Motor/task-motor.md)
