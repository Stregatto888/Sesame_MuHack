# TaskWeb — Detail

**Priority:** 1 · **Stack:** 8 KB · **Loop period:** ~1 ms

Owns the DNS captive-portal server and the HTTP server. All 8 routes are dispatched here. The hack-lock state machine lives entirely inside this task.

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

## Hack-lock state machine

| State        | Trigger                     | Effect                                  |
| ------------ | --------------------------- | --------------------------------------- |
| **Unlocked** | —                           | Any client can push commands            |
| **Locked**   | `hack` from any client      | Only `hackOwnerIP` can push commands    |
| **Released** | `muhack` from `hackOwnerIP` | Back to unlocked                        |
| —            | `muhack` from non-owner     | `ERROR: only the hacker` — stays locked |

## Related diagrams

- [System overview](../Architecture/architecture-overview.md)
- [TaskDisplay detail](../Display/task-display.md)
- [TaskMotor detail](../Motor/task-motor.md)
