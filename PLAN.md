# Sesame MuHack — Piano di Refactoring

Questo file riassume la conversazione di progettazione e documenta il piano
completo di refactoring incrementale del firmware Sesame MuHack.

---

## Contesto

Il firmware gira su una **Lolin S2 Mini (ESP32-S2)**, processore single-core,
compilato con PlatformIO / framework Arduino + espressif32.

**Hardware:**

- 8 servo su GPIO `{1, 2, 4, 6, 8, 10, 13, 14}` via `ESP32Servo`
- SSD1306 128×64 OLED su I²C (SDA=35, SCL=33, addr `0x3C`) via `Adafruit SSD1306`
- Access Point WiFi `Sesame MuHack` / `12345678`, IP statico `192.168.4.1`
- Captive portal + terminale web + API JSON su porta 80

**Stato di partenza:** tutto in un unico `src/main.cpp` (~1000 righe) con
header monolitici `captive-portal.h`, `face-bitmaps.h`, `movement-sequences.h`.

---

## Strategia: Plan A — C-style namespaces, niente RTOS per ora

Approccio scelto: estrarre il codice in moduli con namespace C++ (`Motors::`,
`Display::`, `Web::`, …) **senza toccare FreeRTOS** nelle prime fasi.
Ogni fase produce un commit su `develop`, con build verificata a zero errori
prima del commit.

Stile delle fasi:

1. **Una cosa per volta.** Ogni fase ha uno scope preciso e limitato.
2. **Compile-first.** Nessun commit senza `pio run` verde.
3. **No regressioni.** Il robot si deve comportare identicamente dopo ogni fase.

---

## Fasi

### ✅ Phase 0 — Estrazione costanti

**Commit:** `a1590bf`

**Cosa è cambiato:**

- Creato `include/core/config.h` con tutte le `#define` (pin servo, credenziali
  AP, IP, hostname, timing defaults, costanti marquee).
- Eliminati i folder vuoti `application/` e `hal/`.
- `main.cpp`: sostituiti tutti i magic numbers con le costanti.

**Invariante:** zero cambiamenti di comportamento.

---

### ✅ Phase 1 — Modulo Motors

**Commit:** `94fc06d`

**File creati / modificati:**
| File | Contenuto |
|---|---|
| `include/motors/servo_driver.h` | Enum `ServoName`, `FaceAnimMode`; namespace `Motors::` con `init()`, `setAngle()`, `subtrim[]` |
| `src/motors/servo_driver.cpp` | Hardware driver servo: timer PWM, attach sequence, angle write con subtrim |
| `include/motors/poses.h` | 19 prototipi `run*Pose` / `run*Gait` |
| `src/motors/poses.cpp` | 19 corpi delle funzioni di posa/gait (estratti dall'header) |
| `include/movement-sequences.h` | **Eliminato** (sostituito dai due file sopra) |
| `src/main.cpp` | Servo globals rimossi; `setServoAngle` → `Motors::setAngle`; init → `Motors::init()` |

**Externs temporanei in `poses.cpp`** (da rimuovere nelle fasi successive):

```cpp
extern int    frameDelay;
extern int    walkCycles;
extern int    motorCurrentDelay;
extern String currentCommand;
extern void   delayWithFace(unsigned long ms);
extern bool   pressingCheck(String cmd, int ms);
```

---

### ✅ Phase 2 — Modulo Display

**Commit:** `c286bc3`

**File creati / modificati:**
| File | Contenuto |
|---|---|
| `include/display/bitmaps.h` | Asset PROGMEM bitmap (da `include/face-bitmaps.h`, eliminato) |
| `include/display/face_engine.h` | Enum `FaceAnimMode`; namespace `Display::` con `init()`, `set()`, `setMode()`, `setWithMode()`, `setMarqueeText()`, `bootMsg()`, `tickFace()`, `tickIdle()`, `tickMarquee()`, `enterIdle()`, `exitIdle()`, `notifyInput()`; extern `currentFaceName`, `faceFps` |
| `src/display/face_engine.cpp` | Tutto lo stato display (frame face, FSM idle/blink, marquee), logica render, istanza SSD1306 privata |
| `include/motors/servo_driver.h` | `FaceAnimMode` rimosso (ora in `face_engine.h`) |
| `src/motors/poses.cpp` | `extern` display rimossi; aggiunto `#include "display/face_engine.h"`; chiamate aggiornate a `Display::setWithMode` / `Display::enterIdle` |
| `src/main.cpp` | Rimosso `Adafruit_SSD1306 display`, tutti i globals face, tutti i corpi funzione display; loop/helper aggiornati a `Display::tick*` / `Display::notifyInput()` |

---

### ✅ Phase 3 — Modulo Web

**Commit:** `1364365`

**File creati / modificati:**
| File | Contenuto |
|---|---|
| `include/web/web_assets.h` | Spostato da `include/captive-portal.h` — HTML/CSS/JS PROGMEM |
| `include/web/web_server.h` | Namespace `Web::`: `init()`, `pump()`, extern `networkConnected`, `networkIP`, `deviceHostname` |
| `src/web/web_server.cpp` | Tutti gli handler HTTP, istanze `WebServer`/`DNSServer`, stato hack-lock (static privato), init mDNS |
| `src/main.cpp` | Rimossi ~400 righe; `setup()` chiama `Web::init(apOk, myIP)`; `loop()`/helpers chiamano `Web::pump()` |

**Decisioni di design:**

- `Web::networkConnected/IP/deviceHostname` definiti in `web_server.cpp`, settati in `setup()` dopo STA.
- `currentCommand`, `frameDelay`, `walkCycles`, `motorCurrentDelay` restano in `main.cpp`; `web_server.cpp` li prende via `extern`.
- `hackLocked`, `hackOwnerIP`, `hackOwnerMAC` sono ora `static` in `web_server.cpp` — completamente privati.
- mDNS spostato dentro `Web::init()`.

**Build:** RAM 17.0% / Flash 71.4%

---

### 🔴 Phase 4 — FreeRTOS Queue Plumbing

**Commit:** _da fare_

**Obiettivo:** Introdurre una `FreeRTOS queue` come canale di comunicazione
tra il modulo Web (producer) e il dispatcher comandi (consumer), senza ancora
creare task separati. Il loop principale resta come scheduler cooperativo.

**Cambiamenti previsti:**

- Creare `include/core/command_queue.h` con `CommandQueue::` namespace:
  - `void init()` — crea la queue con `xQueueCreate`
  - `bool push(const String& cmd)` — `xQueueSendToBack` (da ISR/handler web)
  - `bool pop(String& cmd)` — `xQueueReceive` con timeout 0 (non bloccante)
- `src/core/command_queue.cpp` — implementazione
- `src/web/web_server.cpp` — sostituire `extern String currentCommand` con
  `CommandQueue::push(cmd)` in tutti gli handler
- `src/main.cpp` — `loop()` usa `CommandQueue::pop(currentCommand)` invece di
  leggere direttamente `currentCommand`
- Rimuovere `extern String currentCommand` da `web_server.cpp`

**Externs da eliminare in questa fase:**

```cpp
// in web_server.cpp
extern String currentCommand;   // → CommandQueue::push()
```

**Nota:** `currentCommand` resta come variabile locale al dispatcher in
`main.cpp` (o in un futuro `MotorTask`), ma non è più shared state globale
scritto da web.

---

### 🔴 Phase 5 — Split in 3 FreeRTOS Task

**Commit:** _da fare_

**Obiettivo:** Spostare le tre responsabilità principali su task FreeRTOS
separati. L'ESP32-S2 è single-core, quindi i task si dividono il tempo CPU
cooperativamente — ma la struttura è pronta per dual-core se si cambia board.

**Task previsti:**

| Task          | Priorità  | Stack | Responsabilità                      |
| ------------- | --------- | ----- | ----------------------------------- |
| `TaskWeb`     | 1 (bassa) | 8 KB  | `Web::pump()` + DNS/HTTP            |
| `TaskMotor`   | 2 (media) | 6 KB  | Pop dalla queue, dispatch pose/gait |
| `TaskDisplay` | 1 (bassa) | 4 KB  | `Display::tickFace/Idle/Marquee()`  |

**Cambiamenti previsti:**

- Creare `src/core/tasks.cpp` con le tre funzioni task e `Tasks::startAll()`
- `setup()` chiama `Tasks::startAll()` invece di entrare nel loop classico
- `loop()` ridotto a `vTaskDelete(NULL)` o lasciato vuoto
- `delayWithFace()` e `pressingCheck()` — refactored per non pompare web
  direttamente (il `TaskWeb` ci pensa da solo)
- Rimuovere gli externs rimanenti tra moduli:
  ```cpp
  // in poses.cpp — da eliminare
  extern int    frameDelay;
  extern int    walkCycles;
  extern int    motorCurrentDelay;
  extern void   delayWithFace(unsigned long ms);
  extern bool   pressingCheck(String cmd, int ms);
  ```
- `frameDelay`, `walkCycles`, `motorCurrentDelay` diventano stato interno del
  `TaskMotor` (o del modulo `Motors::`) con accessor thread-safe

---

### 🔴 Phase 6 — Thread-Safety Hardening

**Commit:** _da fare_

**Obiettivo:** Rendere il codice sicuro per accesso concorrente ai dati
condivisi tra task.

**Aree critiche da proteggere:**

| Variabile / risorsa                             | Accesso attuale                         | Soluzione prevista                     |
| ----------------------------------------------- | --------------------------------------- | -------------------------------------- |
| `currentCommand`                                | web scrive, motor legge                 | già risolto con queue in Phase 4       |
| `Display::currentFaceName`                      | motor scrive, web legge (status API)    | `SemaphoreHandle_t` mutex              |
| `Display::faceFps`                              | web scrive (setSettings), display legge | `SemaphoreHandle_t` mutex              |
| `frameDelay`, `walkCycles`, `motorCurrentDelay` | web scrive, motor legge                 | mutex o `std::atomic<int>`             |
| SSD1306 I²C bus                                 | display task scrive continuamente       | già privato a `face_engine.cpp`, safe  |
| Servo PWM                                       | motor task scrive                       | già privato a `servo_driver.cpp`, safe |

**Strumenti FreeRTOS da usare:**

- `SemaphoreHandle_t` / `xSemaphoreTake` / `xSemaphoreGive` per dati condivisi
- `portENTER_CRITICAL` / `portEXIT_CRITICAL` per sezioni brevissime (< 10 µs)
- Review finale con `configUSE_TRACE_FACILITY` o Segger SystemView se disponibile

---

## Struttura target (dopo Phase 6)

```
src/
  main.cpp                  ← solo setup() + Tasks::startAll()
  core/
    command_queue.cpp       ← FreeRTOS queue wrapper
    tasks.cpp               ← definizione dei 3 task FreeRTOS
  motors/
    servo_driver.cpp        ← hardware driver servo
    poses.cpp               ← pose e gait
  display/
    face_engine.cpp         ← OLED renderer + FSM idle/marquee
  web/
    web_server.cpp          ← HTTP + DNS + hack-lock

include/
  core/
    config.h                ← tutte le costanti
    command_queue.h
  motors/
    servo_driver.h
    poses.h
  display/
    face_engine.h
    bitmaps.h
  web/
    web_server.h
    web_assets.h
```

---

## Stato avanzamento

| Fase                    | Status | Commit    |
| ----------------------- | ------ | --------- |
| 0 — Estrazione costanti | ✅     | `a1590bf` |
| 1 — Modulo Motors       | ✅     | `94fc06d` |
| 2 — Modulo Display      | ✅     | `c286bc3` |
| 3 — Modulo Web          | ✅     | `1364365` |
| 4 — FreeRTOS Queue      | 🔴     | —         |
| 5 — 3 FreeRTOS Task     | 🔴     | —         |
| 6 — Thread-Safety       | 🔴     | —         |

---

## Note tecniche importanti

- **`WiFi.persistent(false)`** va chiamato PRIMA di `softAP()` sull'ESP32-S2,
  altrimenti il chip entra in reboot loop se la NVS è corrotta.
- **`delay(2000)` dopo `softAP()`** — l'interfaccia di rete dell'S2 ha bisogno
  di ~2 secondi per diventare operativa prima di poter chiamare `softAPConfig()`.
- **`softAPIP()` NON va chiamato** sull'S2 — può bloccarsi. Usare sempre la
  costante `AP_IP`.
- **`delayWithFace()`** e **`pressingCheck()`** chiamano `Web::pump()`
  internamente — questo permette al server HTTP di rispondere anche durante
  le animazioni bloccanti.
- Il pattern ODR usato: variabile definita una sola volta nel `.cpp`, dichiarata
  `extern` nell'header.
