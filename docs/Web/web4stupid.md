# TaskWeb — How It Works

**Priority:** 1 · **Stack:** 8 KB · **Loop period:** every ~1 ms

Manages the captive portal DNS server and the HTTP server. All 8 routes are handled here. The hack-lock mechanism lives entirely inside this task.

```mermaid
flowchart TD
    START([Web Loop Start]) --> PUMP["Listen on Network\n(Wait for connections)"]

    PUMP --> DNS_PROC["Handle DNS requests\n(Ignore if none)"]
    DNS_PROC --> HTTP_PROC["Handle HTTP requests\n(Take one user request)"]
    HTTP_PROC --> ROUTE{What does\nthe user want?}

    ROUTE -->|Opens the site| ROOT["Send the Web Page\n(Graphical interface)"]
    ROUTE -->|Movement button| CMD["Simple Command\n(e.g. Walk, Stop)"]
    ROUTE -->|Web terminal| TERM["Advanced Command\n(Text from web terminal)"]
    ROUTE -->|External integration| APICMD["API Command\n(Structured JSON data)"]
    ROUTE -->|Status request| STATUS["Send Current Status\n(What am I doing? IP?)"]
    ROUTE -->|Parameter request| GETS["Send Settings\n(Current speeds)"]
    ROUTE -->|Save parameters| SETS["Update Settings\n(Apply new speeds)"]
    ROUTE -->|Page not found| ROOT

    TERM --> HACK_CHECK{What command is it?}
    HACK_CHECK -->|Take control| LOCK["Lock the robot\nBind to your IP\n→ Success"]
    HACK_CHECK -->|Release control| UNLOCK{"Are you the owner?"}
    UNLOCK -->|Yes| FREE["Unlock robot for everyone\n→ Success"]
    UNLOCK -->|No| DENY2["→ Error: No permission"]
    HACK_CHECK -->|System info| STAT_RESP["Reply with Info\n(IP, Connections, Lock State)"]
    HACK_CHECK -->|Move the robot| BLOCKED{"Is the robot locked\nby another user?"}
    BLOCKED -->|Yes| DENY["→ Error: Access Denied"]
    BLOCKED -->|No| ENQUEUE["Accept the command\nWake the screen\nPass it to the Motors"]

    APICMD --> PARSE["Parse JSON text"]
    PARSE -->|Expression only| FACE_SET["Change face\nWake the screen"]
    PARSE -->|Expression + Movement| CMD_PUSH["Accept the command\nOptionally change face"]

    SETS --> WRITE["Save new parameters to memory\n(Using protected variables)"]

    ENQUEUE --> DELAY["Very short rest (1ms)\nto avoid blocking the CPU"]
    ROOT --> DELAY
    DELAY --> START

    style LOCK fill:#e63946,color:#fff
    style FREE fill:#3fb950,color:#000
    style DENY fill:#e63946,color:#fff
    style DENY2 fill:#e63946,color:#fff
    style ENQUEUE fill:#2a2a3e,color:#ffd700
    style CMD_PUSH fill:#2a2a3e,color:#ffd700
```

## Hack-lock state machine

Anyone connected to the robot's WiFi network can control it.
The `hack` command lets a single user take exclusive control:

| State | Trigger | Effect |
| --- | --- | --- |
| **Unlocked** | — (starting state) | Any client can send commands |
| **Locked** | `hack` from any client | Only the IP that sent `hack` can command the robot |
| **Released** | `muhack` from the owner IP | Back to unlocked for everyone |
| — | `muhack` from a non-owner IP | `ERROR: only the hacker can unlock` — stays locked |

## Related diagrams

- [System Overview](../Architecture/architecture4stupid.md)
- [TaskDisplay — How It Works](../Display/display4stupid.md)
- [TaskMotor — How It Works](../Motor/motor4stupid.md)
