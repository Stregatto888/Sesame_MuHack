/**
 * @file main.cpp
 * @brief Firmware entry-point and run-time orchestration for the Sesame
 *        MuHack robot.
 * @author Luca - MuHack
 *
 * The Sesame MuHack robot is an eight-servo hobby quadruped powered by a
 * Lolin S2 Mini (ESP32-S2) board, equipped with an SSD1306 128x64 OLED
 * "face" and an embedded Wi-Fi captive portal that exposes a remote
 * control panel and a hacker-themed terminal.
 *
 * ## High-level architecture
 *
 * @code
 *      ┌──────────────────────────────────────────────────────────┐
 *      │                       loop()                              │
 *      │                                                            │
 *      │  ┌────────────┐   ┌──────────────┐   ┌─────────────────┐ │
 *      │  │ DNS / HTTP │   │ Face animator│   │ Idle / scrolling│ │
 *      │  └─────┬──────┘   └──────┬───────┘   └────────┬────────┘ │
 *      │        │                  │                    │          │
 *      │        ▼                  ▼                    ▼          │
 *      │  ┌──────────────────────────────────────────────────────┐ │
 *      │  │              currentCommand dispatcher               │ │
 *      │  │  forward / backward / left / right / pose-by-name    │ │
 *      │  └──────────────────────────────────────────────────────┘ │
 *      │        │                                                  │
 *      │        ▼                                                  │
 *      │  ┌──────────────────────────────────────────────────────┐ │
 *      │  │  Pose / gait routines (movement-sequences.h)         │ │
 *      │  │     ↓                                                │ │
 *      │  │  setServoAngle()  ← drives the eight ESP32Servo      │ │
 *      │  │  setFace*()       ← drives the OLED face renderer    │ │
 *      │  └──────────────────────────────────────────────────────┘ │
 *      └──────────────────────────────────────────────────────────┘
 * @endcode
 *
 * ## Operating modes
 *  - **Soft AP**: always enabled, ESSID `Sesame MuHack`, captive portal
 *    served on `http://192.168.4.1/`.
 *  - **Station mode**: optional, controlled by ::ENABLE_NETWORK_MODE. When
 *    enabled the robot also joins an upstream Wi-Fi network and exposes
 *    itself through mDNS as `sesame-robot.local`.
 *  - **Hack lock**: a single client can claim exclusive control via the
 *    embedded terminal (`hack` to lock, `muhack` to release).
 *
 * @see captive-portal.h
 * @see face-bitmaps.h
 * @see movement-sequences.h
 */

// ============================================================================
// INCLUDES
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
// ESP32Servo.h is included by motors/servo_driver.h transitively via servo_driver.cpp
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "face-bitmaps.h"
#include "captive-portal.h"
#include "core/config.h"
#include "motors/servo_driver.h"
#include "motors/poses.h"

// ============================================================================
// PERIPHERAL INSTANCES
// ============================================================================

/// DNS server intercepting every query so the captive portal auto-pops up.
DNSServer dnsServer;

/// SSD1306 face display.
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/// HTTP server backing the captive portal and JSON command API.
WebServer server(80);

// ============================================================================
// SERVO ROUTING
// ============================================================================
//
// Servo objects, pin mapping and subtrim values are now owned by the
// Motors namespace in src/motors/servo_driver.cpp.
// Access subtrim via Motors::subtrim[]  and servo writes via Motors::setAngle().
//

// ============================================================================
// ANIMATION TIMING
// ============================================================================

int frameDelay = DEFAULT_FRAME_DELAY;             ///< Inter-frame delay used by gait routines (ms).
int walkCycles = DEFAULT_WALK_CYCLES;             ///< Gait repetitions per walk/turn invocation.
int motorCurrentDelay = DEFAULT_MOTOR_CURRENT_DELAY; ///< Pause after each servo write to spread current.

// ============================================================================
// FACE ANIMATION STATE
// ============================================================================

String currentCommand = "";                              ///< Active command from any UI.
String currentFaceName = "default";                      ///< Face symbolic name.
const unsigned char *const *currentFaceFrames = nullptr; ///< Frame array.
uint8_t currentFaceFrameCount = 0;                       ///< Frames in array.
uint8_t currentFaceFrameIndex = 0;                       ///< Index played next.
unsigned long lastFaceFrameMs = 0;                       ///< millis() of last frame.
int faceFps = DEFAULT_FACE_FPS;                          ///< Default FPS fallback.
FaceAnimMode currentFaceMode = FACE_ANIM_LOOP;           ///< Current mode.
int8_t faceFrameDirection = 1;                           ///< 1=fwd, -1=rev.
bool faceAnimFinished = false;                           ///< Once-mode flag.
int currentFaceFps = 0;                                  ///< FPS for active face.

bool idleActive = false;           ///< Idle face active?
bool idleBlinkActive = false;      ///< Currently blinking?
unsigned long nextIdleBlinkMs = 0; ///< Next blink schedule.
uint8_t idleBlinkRepeatsLeft = 0;  ///< Pending double-blinks.

// ============================================================================
// STATUS BAR / WIFI INFO SCROLLING
// ============================================================================

unsigned long lastInputTime = 0;    ///< millis() of last received input.
bool firstInputReceived = false;    ///< Set true after any user input.
bool showingWifiInfo = false;       ///< Marquee currently active?
int wifiScrollPos = 0;              ///< Marquee X offset, in pixels.
unsigned long lastWifiScrollMs = 0; ///< millis() of last marquee tick.
String wifiInfoText = "";           ///< Pre-rendered marquee text.

// ============================================================================
// NETWORK / HACK-LOCK STATE
// ============================================================================

bool networkConnected = false;          ///< STA association status.
IPAddress networkIP;                    ///< IP obtained on the upstream LAN.
String deviceHostname = DEVICE_HOSTNAME; ///< mDNS hostname.

bool hackLocked = false;  ///< Exclusive-control flag.
IPAddress hackOwnerIP;    ///< Client owning the lock.
String hackOwnerMAC = ""; ///< Reserved for future MAC lock.

// ============================================================================
// FACE DISPATCH TABLE (built from FACE_LIST)
// ============================================================================

/**
 * @struct FaceEntry
 * @brief Bind a symbolic face name to its bitmap frame array.
 */
struct FaceEntry
{
  const char *name;                   ///< Lowercase identifier.
  const unsigned char *const *frames; ///< Pointer to the frame array.
  uint8_t maxFrames;                  ///< Capacity of @ref frames.
};

// MAX_FACE_FRAMES is defined in core/config.h

/// Expand an `FACE_LIST` entry into a six-slot frame array.
#define MAKE_FACE_FRAMES(name)                                         \
  const unsigned char *const face_##name##_frames[] = {                \
      epd_bitmap_##name, epd_bitmap_##name##_1, epd_bitmap_##name##_2, \
      epd_bitmap_##name##_3, epd_bitmap_##name##_4, epd_bitmap_##name##_5};

#define X(name) MAKE_FACE_FRAMES(name)
FACE_LIST
#undef X
#undef MAKE_FACE_FRAMES

/// Runtime registry mapping face names to their frame arrays.
const FaceEntry faceEntries[] = {
#define X(name) {#name, face_##name##_frames, MAX_FACE_FRAMES},
    FACE_LIST
#undef X
    {"default", face_defualt_frames, MAX_FACE_FRAMES}};

/**
 * @struct FaceFpsEntry
 * @brief Override default playback rate for a single face.
 */
struct FaceFpsEntry
{
  const char *name; ///< Face identifier (lowercase).
  uint8_t fps;      ///< Frame-rate to apply to @ref name.
};

/// Per-face FPS overrides. Faces missing here fall back to ::faceFps.
const FaceFpsEntry faceFpsEntries[] = {
    {"walk", 1},
    {"rest", 1},
    {"swim", 1},
    {"dance", 1},
    {"wave", 1},
    {"point", 5},
    {"stand", 1},
    {"cute", 1},
    {"pushup", 1},
    {"freaky", 1},
    {"bow", 1},
    {"worm", 1},
    {"shake", 1},
    {"shrug", 1},
    {"dead", 2},
    {"crab", 1},
    {"idle", 1},
    {"idle_blink", 7},
    {"default", 1},
    // Conversational faces are driven externally (no auto-animation).
    {"happy", 1},
    {"talk_happy", 1},
    {"sad", 1},
    {"talk_sad", 1},
    {"angry", 1},
    {"talk_angry", 1},
    {"surprised", 1},
    {"talk_surprised", 1},
    {"sleepy", 1},
    {"talk_sleepy", 1},
    {"love", 1},
    {"talk_love", 1},
    {"excited", 1},
    {"talk_excited", 1},
    {"confused", 1},
    {"talk_confused", 1},
    {"thinking", 1},
    {"talk_thinking", 1},
};

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void updateFaceBitmap(const unsigned char *bitmap);
void setFace(const String &faceName);
void setFaceMode(FaceAnimMode mode);
void setFaceWithMode(const String &faceName, FaceAnimMode mode);
void updateAnimatedFace();
void delayWithFace(unsigned long ms);
void enterIdle();
void exitIdle();
void updateIdleBlink();
int getFaceFpsForName(const String &faceName);
bool pressingCheck(String cmd, int ms);
void handleGetSettings();
void handleSetSettings();
void handleGetStatus();
void handleApiCommand();
void updateWifiInfoScroll();
void recordInput();
void handleTerminalCmd();
bool isHackBlocked();

// ============================================================================
// HTTP HANDLERS - CAPTIVE PORTAL
// ============================================================================

/**
 * @brief Serve the embedded captive-portal landing page.
 * @see captive-portal.h
 */
void handleRoot()
{
  server.send(200, "text/html", index_html);
}

// ============================================================================
// HACK-LOCK HELPERS
// ============================================================================

/**
 * @brief Check whether the current HTTP request is denied by the hack lock.
 *
 * When @ref hackLocked is set, only the IP address recorded in
 * @ref hackOwnerIP is allowed to drive the robot.
 *
 * @return `true` if the request comes from a non-owner client while the
 *         lock is engaged.
 */
bool isHackBlocked()
{
  if (!hackLocked)
    return false;
  IPAddress clientIP = server.client().remoteIP();
  return (clientIP != hackOwnerIP);
}

// ============================================================================
// HTTP HANDLERS - TERMINAL & COMMAND API
// ============================================================================

/**
 * @brief Handle text commands issued through the embedded terminal page.
 *
 * Recognises the following keywords:
 *  - `hack`    : take exclusive control of the robot.
 *  - `muhack`  : release exclusive control (only by the lock owner).
 *  - `status`  : dump the current runtime state.
 *  - `help`    : list available commands.
 *  - movement  : `forward`, `backward`, `left`, `right`, `stop`.
 *  - poses     : `rest`, `stand`, `wave`, ... (see ::validPoses below).
 *
 * Replies are JSON-encoded and consumed by the JavaScript front-end.
 */
void handleTerminalCmd()
{
  if (!server.hasArg("cmd"))
  {
    server.send(400, "application/json", "{\"error\":\"Missing cmd\"}");
    return;
  }
  String cmd = server.arg("cmd");
  cmd.trim();
  Serial.print(F("[DEBUG] handleTerminalCmd: Command received -> "));
  Serial.println(cmd);
  String cmdLower = cmd;
  cmdLower.toLowerCase();

  IPAddress clientIP = server.client().remoteIP();

  // --- HACK command: take exclusive control ---
  if (cmdLower == "hack")
  {
    hackLocked = true;
    hackOwnerIP = clientIP;
    Serial.print("[HACK] Control locked by: ");
    Serial.println(clientIP);
    server.send(200, "application/json", "{\"response\":\"ACCESS GRANTED. You now have exclusive control.\\nAll other users are locked out.\\nType 'muhack' to release control.\",\"locked\":true}");
    return;
  }

  // --- MUHACK command: release exclusive control ---
  if (cmdLower == "muhack")
  {
    if (hackLocked && clientIP == hackOwnerIP)
    {
      hackLocked = false;
      hackOwnerIP = IPAddress(0, 0, 0, 0);
      Serial.println("[MUHACK] Control released.");
      server.send(200, "application/json", "{\"response\":\"LOCK RELEASED. All users can now control the robot.\\nMuHack - Brescia Hackerspace\",\"locked\":false}");
    }
    else if (hackLocked)
    {
      server.send(200, "application/json", "{\"response\":\"ERROR: Only the hacker who locked can release.\\nAsk them to type 'muhack'.\",\"locked\":true}");
    }
    else
    {
      server.send(200, "application/json", "{\"response\":\"Robot is already free. No lock active.\\nMuHack - Brescia Hackerspace\",\"locked\":false}");
    }
    return;
  }

  // --- STATUS command ---
  if (cmdLower == "status")
  {
    String resp = "IP: " + WiFi.softAPIP().toString();
    resp += "\\nSSID: " + String(AP_SSID);
    resp += "\\nClients: " + String(WiFi.softAPgetStationNum());
    resp += "\\nCommand: " + (currentCommand.length() > 0 ? currentCommand : String("idle"));
    resp += "\\nFace: " + currentFaceName;
    resp += "\\nHack Lock: " + String(hackLocked ? "ACTIVE" : "off");
    if (hackLocked)
      resp += "\\nOwner: " + hackOwnerIP.toString();
    server.send(200, "application/json", "{\"response\":\"" + resp + "\"}");
    return;
  }

  // --- HELP command ---
  if (cmdLower == "help")
  {
    String resp = "=== SESAME TERMINAL ===";
    resp += "\\nMovement: forward, backward, left, right, stop";
    resp += "\\nPoses: rest, stand, wave, dance, swim, point";
    resp += "\\n       pushup, bow, cute, freaky, worm, shake";
    resp += "\\n       shrug, dead, crab";
    resp += "\\nSystem: status, help, hack, muhack";
    resp += "\\n";
    resp += "\\nhack   - Take exclusive control";
    resp += "\\nmuhack - Release control to all";
    server.send(200, "application/json", "{\"response\":\"" + resp + "\"}");
    return;
  }

  // --- For all other commands, check hack lock ---
  if (isHackBlocked())
  {
    server.send(200, "application/json", "{\"response\":\"ACCESS DENIED. Robot is under exclusive control.\\nWait for 'muhack' to be issued.\",\"locked\":true}");
    return;
  }

  // --- Movement commands ---
  if (cmdLower == "forward" || cmdLower == "backward" || cmdLower == "left" || cmdLower == "right")
  {
    currentCommand = cmdLower;
    recordInput();
    exitIdle();
    server.send(200, "application/json", "{\"response\":\"Executing: " + cmdLower + "\"}");
    return;
  }

  if (cmdLower == "stop")
  {
    currentCommand = "";
    recordInput();
    server.send(200, "application/json", "{\"response\":\"All movement stopped.\"}");
    return;
  }

  // --- Pose commands ---
  String validPoses[] = {"rest", "stand", "wave", "dance", "swim", "point", "pushup", "bow", "cute", "freaky", "worm", "shake", "shrug", "dead", "crab"};
  for (int i = 0; i < 15; i++)
  {
    if (cmdLower == validPoses[i])
    {
      currentCommand = cmdLower;
      recordInput();
      exitIdle();
      server.send(200, "application/json", "{\"response\":\"Pose: " + cmdLower + "\"}");
      return;
    }
  }

  // --- Unknown command ---
  server.send(200, "application/json", "{\"response\":\"Unknown command: " + cmd + "\\nType 'help' for available commands.\"}");
}

/**
 * @brief Handle the legacy URL-encoded command endpoint used by the web UI.
 *
 * Accepts the following query parameters (mutually exclusive):
 *  - `pose=<name>`           : run a named pose.
 *  - `go=<dir>`              : start a movement gait.
 *  - `stop=1`                : abort the current command.
 *  - `motor=<id>&value=<a>`  : drive a single servo to angle @c a.
 */
void handleCommandWeb()
{
  // Check hack lock for web UI commands too
  if (isHackBlocked())
  {
    server.send(403, "text/plain", "LOCKED");
    return;
  }
  // We send 200 OK immediately so the web browser doesn't hang waiting for animation to finish
  if (server.hasArg("pose"))
  {
    currentCommand = server.arg("pose");
    recordInput();
    exitIdle();
    server.send(200, "text/plain", "OK");
  }
  else if (server.hasArg("go"))
  {
    currentCommand = server.arg("go");
    recordInput();
    exitIdle();
    server.send(200, "text/plain", "OK");
  }
  else if (server.hasArg("stop"))
  {
    currentCommand = "";
    recordInput();
    server.send(200, "text/plain", "OK");
  }
  else if (server.hasArg("motor") && server.hasArg("value"))
  {
    int motorNum = server.arg("motor").toInt();
    int servoIdx = servoNameToIndex(server.arg("motor"));
    int angle = server.arg("value").toInt();
    if (motorNum >= 1 && motorNum <= 8 && angle >= 0 && angle <= 180)
    {
      Motors::setAngle(motorNum - 1, angle); // Convert 1-based to 0-based index
      recordInput();
      server.send(200, "text/plain", "OK");
    }
    else if (servoIdx != -1 && angle >= 0 && angle <= 180)
    {
      Motors::setAngle(servoIdx, angle);
      recordInput();
      server.send(200, "text/plain", "OK");
    }
    else
    {
      server.send(400, "text/plain", "Invalid motor or angle");
    }
  }
  else
  {
    server.send(400, "text/plain", "Bad Args");
  }
}

/**
 * @brief Return the live tunable parameters as JSON.
 */
void handleGetSettings()
{
  String json = "{";
  json += "\"frameDelay\":" + String(frameDelay) + ",";
  json += "\"walkCycles\":" + String(walkCycles) + ",";
  json += "\"motorCurrentDelay\":" + String(motorCurrentDelay) + ",";
  json += "\"faceFps\":" + String(faceFps);
  json += "}";
  server.send(200, "application/json", json);
}

/**
 * @brief Apply tunable parameters received as URL-encoded form fields.
 *
 * Recognised keys: `frameDelay`, `walkCycles`, `motorCurrentDelay`,
 * `faceFps`. Unknown keys are silently ignored.
 */
void handleSetSettings()
{
  if (server.hasArg("frameDelay"))
    frameDelay = server.arg("frameDelay").toInt();
  if (server.hasArg("walkCycles"))
    walkCycles = server.arg("walkCycles").toInt();
  if (server.hasArg("motorCurrentDelay"))
    motorCurrentDelay = server.arg("motorCurrentDelay").toInt();
  if (server.hasArg("faceFps"))
    faceFps = (int)max(1L, server.arg("faceFps").toInt());
  server.send(200, "text/plain", "OK");
}

/**
 * @brief Expose the runtime status as a JSON document for remote clients.
 */
void handleGetStatus()
{
  String json = "{";
  json += "\"currentCommand\":\"" + currentCommand + "\",";
  json += "\"currentFace\":\"" + currentFaceName + "\",";
  json += "\"networkConnected\":" + String(networkConnected ? "true" : "false") + ",";
  json += "\"apIP\":\"" + WiFi.softAPIP().toString() + "\"";
  if (networkConnected)
  {
    json += ",\"networkIP\":\"" + networkIP.toString() + "\"";
  }
  json += "}";
  server.send(200, "application/json", json);
}

/**
 * @brief JSON command endpoint for network-side clients.
 *
 * Expects a POST with one of:
 *  - `{"command": "<name>", "face": "<face>"}`
 *  - `{"face": "<face>"}` (face-only update; no movement triggered)
 *
 * The body parser is intentionally minimal: it does not depend on a JSON
 * library and tolerates whitespace variants of `"command":` / `"face":`.
 */
void handleApiCommand()
{
  if (server.method() != HTTP_POST)
  {
    server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
    return;
  }

  String body = server.arg("plain");

  Serial.println("API Command received:");
  Serial.println(body);

  // Check for face-only command (no movement)
  int faceOnlyStart = body.indexOf("\"face\":\"");
  if (faceOnlyStart == -1)
  {
    faceOnlyStart = body.indexOf("\"face\": \"");
  }

  // If we have a face but no command field, it's face-only
  bool faceOnly = (faceOnlyStart > 0 && body.indexOf("\"command\":") == -1 && body.indexOf("\"command\": ") == -1);

  String command = "";
  String face = "";

  // Parse face
  if (faceOnlyStart > 0)
  {
    faceOnlyStart = body.indexOf("\"", faceOnlyStart + 6) + 1;
    int faceEnd = body.indexOf("\"", faceOnlyStart);
    if (faceEnd > faceOnlyStart)
    {
      face = body.substring(faceOnlyStart, faceEnd);
      Serial.print("Parsed face: ");
      Serial.println(face);
    }
  }

  // Parse command (if not face-only)
  if (!faceOnly)
  {
    int cmdStart = body.indexOf("\"command\":\"");
    if (cmdStart == -1)
    {
      cmdStart = body.indexOf("\"command\": \"");
    }

    if (cmdStart == -1)
    {
      Serial.println("Error: command field not found");
      server.send(400, "application/json", "{\"error\":\"Missing command field\"}");
      return;
    }

    cmdStart = body.indexOf("\"", cmdStart + 10) + 1;
    int cmdEnd = body.indexOf("\"", cmdStart);

    if (cmdEnd <= cmdStart)
    {
      Serial.println("Error: invalid command format");
      server.send(400, "application/json", "{\"error\":\"Invalid command format\"}");
      return;
    }

    command = body.substring(cmdStart, cmdEnd);
    Serial.print("Parsed command: ");
    Serial.println(command);
  }

  // Set face if provided
  if (face.length() > 0)
  {
    setFace(face);
  }

  // If face-only, just acknowledge
  if (faceOnly)
  {
    recordInput();
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Face updated\"}");
    return;
  }

  // Execute command
  if (command == "stop")
  {
    currentCommand = "";
    recordInput();
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Command stopped\"}");
  }
  else
  {
    currentCommand = command;
    recordInput();
    exitIdle();
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Command executed\"}");
  }
}

// ============================================================================
// BOOT / DISPLAY UTILITIES
// ============================================================================

/**
 * @brief Print a boot-time message both on the serial console and the OLED.
 *
 * @param msg    Null-terminated string to display.
 * @param clear  When `true` clear the OLED frame buffer before drawing.
 */
void displayBootMsg(const char *msg, bool clear = false)
{
  if (clear)
  {
    display.clearDisplay();
    display.setCursor(0, 0);
  }
  display.println(msg);
  display.display();
  Serial.println(msg);
}

// ============================================================================
// ARDUINO ENTRY POINTS
// ============================================================================

/**
 * @brief One-shot initialization: serial, I²C, OLED, Wi-Fi, mDNS, HTTP, servos.
 *
 * The boot sequence is intentionally non-blocking: the robot continues
 * even if the OLED is missing or the AP fails to come up, because the
 * physical control loop only depends on the servos.
 */
void setup()
{
  randomSeed(micros());

  // I2C Init for ESP32
  Wire.begin(I2C_SDA, I2C_SCL);

  // OLED Init (non-blocking: robot works even without display)
  bool displayOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
  if (!displayOk)
  {
    Serial.println(F("SSD1306 allocation failed - continuing without display."));
  }

  if (displayOk)
  {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    displayBootMsg("Sesame MuHack Boot", false);
  }

  // === ACCESS POINT ===
  // ESP32-S2 + Arduino-ESP32 v3 reliable AP sequence:
  //
  // CRITICAL: WiFi.persistent(false) must be called first.
  //   Arduino-ESP32 by default saves/loads WiFi credentials from NVS (flash).
  //   If NVS is corrupted (e.g. from multiple firmware flashes), the IDF event
  //   loop panics during softAP() and the watchdog resets the chip → reboot loop.
  //   persistent(false) disables all NVS access for WiFi entirely.
  //
  // Sequence:
  //   1. persistent(false) — disable NVS, must be FIRST
  //   2. softAP()          — handles WiFi init and netif creation
  //   3. delay(2000)       — S2 needs more time than S1 for netif to come up
  //   4. softAPConfig()    — configure IP only after netif is ready
  //   (never call softAPIP() — can hang on S2; use AP_IP constant instead)
  if (displayOk)
    displayBootMsg("Starting AP...");

  IPAddress myIP = AP_IP; // Known static IP, never queried from driver

  WiFi.persistent(false); // Disable NVS read/write — prevents crash on corrupted flash
  WiFi.disconnect(true);  // Reset any leftover WiFi state from previous boot
  delay(200);

  Serial.println("Calling WiFi.softAP...");
  bool apOk = WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("softAP returned: ");
  Serial.println(apOk ? "true" : "false");

  if (apOk)
  {
    delay(2000);                                     // S2 netif needs ~2s to fully come up after softAP() returns
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET); // Set custom IP now that netif is ready
    Serial.print("AP Created. SSID: ");
    Serial.print(AP_SSID);
    Serial.print("  IP: ");
    Serial.println(myIP);
    Serial.println(F("[DEBUG] setup: WiFi Access Point initialized and ready"));
    if (displayOk)
    {
      display.print("AP OK: ");
      displayBootMsg(AP_SSID);
    }
  }
  else
  {
    Serial.println("WARNING: AP failed, retrying...");
    if (displayOk)
      displayBootMsg("AP FAILED! Retrying...");
    delay(1000);
    apOk = WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("softAP retry returned: ");
    Serial.println(apOk ? "true" : "false");
    if (apOk)
    {
      delay(500);
      WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
      Serial.println("AP Created on retry.");
      if (displayOk)
        displayBootMsg("AP OK (retry)");
    }
    else
    {
      Serial.println("AP creation failed permanently. Robot will work offline.");
      if (displayOk)
        displayBootMsg("AP FAILED - offline mode");
    }
  }

  // === OPTIONAL: Station mode (non-blocking) ===
  if (ENABLE_NETWORK_MODE && String(NETWORK_SSID).length() > 0)
  {
    if (displayOk)
      displayBootMsg("Connecting WiFi...");
    Serial.println("Attempting to connect to network: " + String(NETWORK_SSID));

    // Switch to AP+STA mode — softAP() must be re-issued after mode change
    WiFi.mode(WIFI_AP_STA);
    delay(200);
    WiFi.softAP(AP_SSID, AP_PASS);
    delay(500);
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

    WiFi.begin(NETWORK_SSID, NETWORK_PASS);

    // Non-blocking wait: max 5 seconds
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10)
    {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      networkConnected = true;
      networkIP = WiFi.localIP();
      Serial.println();
      Serial.print("Connected to network! IP: ");
      Serial.println(networkIP);
      if (displayOk)
      {
        display.print("Net: ");
        displayBootMsg(networkIP.toString().c_str());
      }
    }
    else
    {
      Serial.println();
      Serial.println("Network connection failed - AP still active, continuing.");
      if (displayOk)
        displayBootMsg("Net: skip (AP only)");
      WiFi.disconnect(false); // Stop STA scanning, keep AP
    }
  }
  else
  {
    Serial.println("Network mode disabled. Running in AP-only mode.");
  }

  // Build WiFi info text for scrolling
  // myIP is already set to the static AP_IP — no need to call softAPIP()
  if (networkConnected)
  {
    wifiInfoText = "AP: " + String(AP_SSID) + " (" + myIP.toString() + ")  |  Network: " + String(NETWORK_SSID) + " (" + networkIP.toString() + ") or " + deviceHostname + ".local  |  ";
  }
  else
  {
    wifiInfoText = "Connect to WiFi: " + String(AP_SSID) + "  |  Pass: " + String(AP_PASS) + "  |  IP: " + myIP.toString() + "  |  Captive Portal will auto-open!  |  ";
  }

  // Initialize input tracking
  lastInputTime = millis();
  firstInputReceived = false;
  showingWifiInfo = false;

  // Start mDNS responder for local network discovery
  if (MDNS.begin(deviceHostname.c_str()))
  {
    Serial.println("mDNS responder started");
    MDNS.addService("http", "tcp", 80);
  }
  else
  {
    Serial.println("mDNS failed - not critical, continuing.");
  }

  // Start DNS Server for Captive Portal
  if (apOk)
  {
    dnsServer.start(DNS_PORT, "*", myIP);
  }

  // Web Server Routes
  server.on("/", handleRoot);
  server.on("/cmd", handleCommandWeb);
  server.on("/getSettings", handleGetSettings);
  server.on("/setSettings", handleSetSettings);
  server.on("/terminal", handleTerminalCmd);
  server.on("/api/status", handleGetStatus);
  server.on("/api/command", handleApiCommand);
  server.onNotFound(handleRoot);
  server.begin();

  // === SERVO INIT (AFTER WiFi to spread current draw over time) ===
  // PWM timer allocation and per-servo stagger are handled inside Motors::init().
  if (displayOk)
    displayBootMsg("Init servos...");
  Motors::init();

  // Show rest face on startup
  if (displayOk)
    setFace("rest");

  Serial.println(F("=== Sesame Robot READY ==="));
  Serial.print(F("AP SSID: "));
  Serial.println(AP_SSID);
  Serial.print(F("AP IP: "));
  Serial.println(myIP);
}

/**
 * @brief Cooperative main loop.
 *
 * Drives, in this order:
 *  -# DNS captive portal pump.
 *  -# HTTP request servicing.
 *  -# OLED face animator and idle-blink scheduler.
 *  -# Wi-Fi info marquee while waiting for the first input.
 *  -# Dispatch of the active command to the matching pose / gait routine.
 *  -# Optional serial CLI for diagnostics and sub-trim tuning.
 */
void loop()
{
  // Process DNS requests for captive portal
  dnsServer.processNextRequest();

  server.handleClient();
  updateAnimatedFace();
  updateIdleBlink();
  updateWifiInfoScroll();

  if (currentCommand != "")
  {
    String cmd = currentCommand;
    Serial.print(F("[DEBUG] loop: dispatching command -> "));
    Serial.println(cmd);
    if (cmd == "forward")
      runWalkPose();
    else if (cmd == "backward")
      runWalkBackward();
    else if (cmd == "left")
      runTurnLeft();
    else if (cmd == "right")
      runTurnRight();
    else if (cmd == "rest")
    {
      runRestPose();
      if (currentCommand == "rest")
        currentCommand = "";
    }
    else if (cmd == "stand")
    {
      runStandPose(1);
      if (currentCommand == "stand")
        currentCommand = "";
    }
    else if (cmd == "wave")
      runWavePose();
    else if (cmd == "dance")
      runDancePose();
    else if (cmd == "swim")
      runSwimPose();
    else if (cmd == "point")
      runPointPose();
    else if (cmd == "pushup")
      runPushupPose();
    else if (cmd == "bow")
      runBowPose();
    else if (cmd == "cute")
      runCutePose();
    else if (cmd == "freaky")
      runFreakyPose();
    else if (cmd == "worm")
      runWormPose();
    else if (cmd == "shake")
      runShakePose();
    else if (cmd == "shrug")
      runShrugPose();
    else if (cmd == "dead")
      runDeadPose();
    else if (cmd == "crab")
      runCrabPose();
  }

  // Serial CLI for debugging (can be used to diagnose servo position issues and wiring)
  if (Serial.available())
  {
    static char command_buffer[32];
    static byte buffer_pos = 0;
    char c = Serial.read();
    if (c == '\n' || c == '\r')
    {
      if (buffer_pos > 0)
      {
        command_buffer[buffer_pos] = '\0';
        int motorNum, angle;
        recordInput();
        if (strcmp(command_buffer, "run walk") == 0 || strcmp(command_buffer, "rn wf") == 0)
        {
          currentCommand = "forward";
          runWalkPose();
          currentCommand = "";
        }
        else if (strcmp(command_buffer, "rn wb") == 0)
        {
          currentCommand = "backward";
          runWalkBackward();
          currentCommand = "";
        }
        else if (strcmp(command_buffer, "rn tl") == 0)
        {
          currentCommand = "left";
          runTurnLeft();
          currentCommand = "";
        }
        else if (strcmp(command_buffer, "rn tr") == 0)
        {
          currentCommand = "right";
          runTurnRight();
          currentCommand = "";
        }
        else if (strcmp(command_buffer, "run rest") == 0 || strcmp(command_buffer, "rn rs") == 0)
          runRestPose();
        else if (strcmp(command_buffer, "run stand") == 0 || strcmp(command_buffer, "rn st") == 0)
          runStandPose(1);
        else if (strcmp(command_buffer, "rn wv") == 0)
        {
          currentCommand = "wave";
          runWavePose();
        }
        else if (strcmp(command_buffer, "rn dn") == 0)
        {
          currentCommand = "dance";
          runDancePose();
        }
        else if (strcmp(command_buffer, "rn sw") == 0)
        {
          currentCommand = "swim";
          runSwimPose();
        }
        else if (strcmp(command_buffer, "rn pt") == 0)
        {
          currentCommand = "point";
          runPointPose();
        }
        else if (strcmp(command_buffer, "rn pu") == 0)
        {
          currentCommand = "pushup";
          runPushupPose();
        }
        else if (strcmp(command_buffer, "rn bw") == 0)
        {
          currentCommand = "bow";
          runBowPose();
        }
        else if (strcmp(command_buffer, "rn ct") == 0)
        {
          currentCommand = "cute";
          runCutePose();
        }
        else if (strcmp(command_buffer, "rn fk") == 0)
        {
          currentCommand = "freaky";
          runFreakyPose();
        }
        else if (strcmp(command_buffer, "rn wm") == 0)
        {
          currentCommand = "worm";
          runWormPose();
        }
        else if (strcmp(command_buffer, "rn sk") == 0)
        {
          currentCommand = "shake";
          runShakePose();
        }
        else if (strcmp(command_buffer, "rn sg") == 0)
        {
          currentCommand = "shrug";
          runShrugPose();
        }
        else if (strcmp(command_buffer, "rn dd") == 0)
        {
          currentCommand = "dead";
          runDeadPose();
        }
        else if (strcmp(command_buffer, "rn cb") == 0)
        {
          currentCommand = "crab";
          runCrabPose();
        }
        else if (strcmp(command_buffer, "subtrim") == 0 || strcmp(command_buffer, "st") == 0)
        {
          Serial.println("Subtrim values:");
          for (int i = 0; i < SERVO_COUNT; i++)
          {
            Serial.print("Motor ");
            Serial.print(i);
            Serial.print(": ");
            if (Motors::subtrim[i] >= 0)
              Serial.print("+");
            Serial.println(Motors::subtrim[i]);
          }
        }
        else if (strcmp(command_buffer, "subtrim save") == 0 || strcmp(command_buffer, "st save") == 0)
        {
          Serial.println("Copy and paste this into your code:");
          Serial.print("int8_t Motors::subtrim[SERVO_COUNT] = {");
          for (int i = 0; i < SERVO_COUNT; i++)
          {
            Serial.print(Motors::subtrim[i]);
            if (i < SERVO_COUNT - 1)
              Serial.print(", ");
          }
          Serial.println("};");
        }
        else if (strncmp(command_buffer, "subtrim reset", 13) == 0 || strncmp(command_buffer, "st reset", 8) == 0)
        {
          for (int i = 0; i < SERVO_COUNT; i++)
            Motors::subtrim[i] = 0;
          Serial.println("All subtrim values reset to 0");
        }
        else if (strncmp(command_buffer, "subtrim ", 8) == 0 || strncmp(command_buffer, "st ", 3) == 0)
        {
          const char *params = (command_buffer[1] == 't') ? command_buffer + 3 : command_buffer + 8;
          int trimMotor, trimValue;
          if (sscanf(params, "%d %d", &trimMotor, &trimValue) == 2)
          {
            if (trimMotor >= 0 && trimMotor < SERVO_COUNT)
            {
              if (trimValue >= -90 && trimValue <= 90)
              {
                Motors::subtrim[trimMotor] = trimValue;
                Serial.print("Motor ");
                Serial.print(trimMotor);
                Serial.print(" subtrim set to ");
                if (trimValue >= 0)
                  Serial.print("+");
                Serial.println(trimValue);
              }
              else
              {
                Serial.println("Subtrim value must be between -90 and +90");
              }
            }
            else
            {
              Serial.println("Invalid motor number (0-7)");
            }
          }
        }
        else if (strncmp(command_buffer, "all ", 4) == 0)
        {
          if (sscanf(command_buffer + 4, "%d", &angle) == 1)
          {
            for (int i = 0; i < SERVO_COUNT; i++)
              Motors::setAngle(i, angle);
            Serial.print("All servos set to ");
            Serial.println(angle);
          }
        }
        else if (sscanf(command_buffer, "%d %d", &motorNum, &angle) == 2)
        {
          if (motorNum >= 0 && motorNum < SERVO_COUNT)
          {
            Motors::setAngle(motorNum, angle);
            Serial.print("Servo ");
            Serial.print(motorNum);
            Serial.print(" set to ");
            Serial.println(angle);
          }
          else
          {
            Serial.println("Invalid motor number (0-7)");
          }
        }
        buffer_pos = 0;
      }
    }
    else if (buffer_pos < sizeof(command_buffer) - 1)
    {
      command_buffer[buffer_pos++] = c;
    }
  }
}

// ============================================================================
// FACE RENDERING
// ============================================================================

/**
 * @brief Push a single 128x64 monochrome bitmap to the OLED.
 */
void updateFaceBitmap(const unsigned char *bitmap)
{
  display.clearDisplay();
  display.drawBitmap(0, 0, bitmap, 128, 64, SSD1306_WHITE);
  display.display();
}

/**
 * @brief Count consecutive non-null frame pointers starting at @p frames.
 *
 * Stops at the first `nullptr` slot, which is how the runtime infers the
 * length of a face animation declared via the `MAKE_FACE_FRAMES` macro.
 *
 * @param frames    Frame pointer array (may be `nullptr`).
 * @param maxFrames Capacity of @p frames.
 * @return Number of valid frames available for playback.
 */
uint8_t countFrames(const unsigned char *const *frames, uint8_t maxFrames)
{
  if (frames == nullptr || frames[0] == nullptr)
    return 0;
  uint8_t count = 0;
  for (uint8_t i = 0; i < maxFrames; i++)
  {
    if (frames[i] == nullptr)
      break;
    count++;
  }
  return count;
}

/**
 * @brief Switch the OLED to the requested face by symbolic name.
 *
 * Resets the per-face animation state and immediately renders the first
 * frame. If @p faceName is unknown the runtime falls back to the
 * `default` face entry.
 *
 * @param faceName Symbolic name (case-insensitive). See @ref FACE_LIST.
 */
void setFace(const String &faceName)
{
  if (faceName == currentFaceName && currentFaceFrames != nullptr)
    return;

  Serial.print(F("[DEBUG] setFace: switching face -> "));
  Serial.println(faceName);
  currentFaceName = faceName;
  currentFaceFrameIndex = 0;
  lastFaceFrameMs = 0;
  faceFrameDirection = 1;
  faceAnimFinished = false;
  currentFaceFps = getFaceFpsForName(faceName);

  currentFaceFrames = face_defualt_frames;
  currentFaceFrameCount = countFrames(face_defualt_frames, MAX_FACE_FRAMES);

  for (size_t i = 0; i < (sizeof(faceEntries) / sizeof(faceEntries[0])); i++)
  {
    if (faceName.equalsIgnoreCase(faceEntries[i].name))
    {
      currentFaceFrames = faceEntries[i].frames;
      currentFaceFrameCount = countFrames(faceEntries[i].frames, faceEntries[i].maxFrames);
      break;
    }
  }

  if (currentFaceFrameCount == 0)
  {
    currentFaceFrames = face_defualt_frames;
    currentFaceFrameCount = countFrames(face_defualt_frames, MAX_FACE_FRAMES);
    currentFaceName = "default";
    currentFaceFps = getFaceFpsForName(currentFaceName);
  }

  if (currentFaceFrameCount > 0 && currentFaceFrames[0] != nullptr)
  {
    updateFaceBitmap(currentFaceFrames[0]);
  }
}

/**
 * @brief Override the playback strategy of the currently displayed face.
 */
void setFaceMode(FaceAnimMode mode)
{
  currentFaceMode = mode;
  faceFrameDirection = 1;
  faceAnimFinished = false;
}

/**
 * @brief Convenience wrapper combining @ref setFaceMode and @ref setFace.
 */
void setFaceWithMode(const String &faceName, FaceAnimMode mode)
{
  setFaceMode(mode);
  setFace(faceName);
}

/**
 * @brief Look up the per-face FPS override.
 *
 * @param faceName Face identifier (case-insensitive).
 * @return The per-face override, or the global ::faceFps fallback.
 */
int getFaceFpsForName(const String &faceName)
{
  for (size_t i = 0; i < (sizeof(faceFpsEntries) / sizeof(faceFpsEntries[0])); i++)
  {
    if (faceName.equalsIgnoreCase(faceFpsEntries[i].name))
    {
      return faceFpsEntries[i].fps;
    }
  }
  return faceFps;
}

/**
 * @brief Advance the face animation according to the active mode.
 *
 * Implements three strategies:
 *  - ::FACE_ANIM_LOOP       : modular increment, never stops.
 *  - ::FACE_ANIM_ONCE       : stops on the last frame and sets
 *                             @ref faceAnimFinished.
 *  - ::FACE_ANIM_BOOMERANG  : ping-pongs between the first and last frame.
 */
void updateAnimatedFace()
{
  if (currentFaceFrames == nullptr || currentFaceFrameCount <= 1)
    return;
  if (currentFaceMode == FACE_ANIM_ONCE && faceAnimFinished)
    return;

  unsigned long now = millis();
  int fps = max(1, (currentFaceFps > 0 ? currentFaceFps : faceFps));
  unsigned long interval = 1000UL / fps;
  if (now - lastFaceFrameMs >= interval)
  {
    lastFaceFrameMs = now;
    if (currentFaceMode == FACE_ANIM_LOOP)
    {
      currentFaceFrameIndex = (currentFaceFrameIndex + 1) % currentFaceFrameCount;
    }
    else if (currentFaceMode == FACE_ANIM_ONCE)
    {
      if (currentFaceFrameIndex + 1 >= currentFaceFrameCount)
      {
        currentFaceFrameIndex = currentFaceFrameCount - 1;
        faceAnimFinished = true;
      }
      else
      {
        currentFaceFrameIndex++;
      }
    }
    else
    {
      if (faceFrameDirection > 0)
      {
        if (currentFaceFrameIndex + 1 >= currentFaceFrameCount)
        {
          faceFrameDirection = -1;
          if (currentFaceFrameIndex > 0)
            currentFaceFrameIndex--;
        }
        else
        {
          currentFaceFrameIndex++;
        }
      }
      else
      {
        if (currentFaceFrameIndex == 0)
        {
          faceFrameDirection = 1;
          if (currentFaceFrameCount > 1)
            currentFaceFrameIndex++;
        }
        else
        {
          currentFaceFrameIndex--;
        }
      }
    }
    updateFaceBitmap(currentFaceFrames[currentFaceFrameIndex]);
  }
}

/**
 * @brief Cooperative @c delay() that keeps DNS, HTTP and face animation alive.
 *
 * Pose / gait routines call this helper instead of `delay()` so that the
 * robot remains responsive (Wi-Fi clients, OLED updates, etc.) while a
 * limb is in motion.
 */
void delayWithFace(unsigned long ms)
{
  unsigned long start = millis();
  while (millis() - start < ms)
  {
    updateAnimatedFace();
    server.handleClient();
    dnsServer.processNextRequest();
    delay(5);
  }
}

// ============================================================================
// IDLE BEHAVIOR
// ============================================================================

/**
 * @brief Schedule the next idle blink within the [@p minMs, @p maxMs] window.
 */
void scheduleNextIdleBlink(unsigned long minMs, unsigned long maxMs)
{
  unsigned long now = millis();
  unsigned long interval = (unsigned long)random(minMs, maxMs);
  nextIdleBlinkMs = now + interval;
}

/**
 * @brief Activate the idle face and arm the random blink scheduler.
 */
void enterIdle()
{
  idleActive = true;
  idleBlinkActive = false;
  idleBlinkRepeatsLeft = 0;
  setFaceWithMode("idle", FACE_ANIM_BOOMERANG);
  scheduleNextIdleBlink(3000, 7000);
}

/**
 * @brief Leave the idle state without altering the currently shown face.
 */
void exitIdle()
{
  idleActive = false;
  idleBlinkActive = false;
}

/**
 * @brief Drive the idle blink state machine.
 *
 * When the idle face is active this routine periodically swaps to the
 * `idle_blink` animation (sometimes a double-blink) before returning to
 * the ambient idle face.
 */
void updateIdleBlink()
{
  if (!idleActive)
    return;

  if (!idleBlinkActive)
  {
    if (millis() >= nextIdleBlinkMs)
    {
      idleBlinkActive = true;
      if (idleBlinkRepeatsLeft == 0 && random(0, 100) < 30)
      {
        idleBlinkRepeatsLeft = 1; // double blink
      }
      setFaceWithMode("idle_blink", FACE_ANIM_ONCE);
    }
    return;
  }

  if (currentFaceMode == FACE_ANIM_ONCE && faceAnimFinished)
  {
    idleBlinkActive = false;
    setFaceWithMode("idle", FACE_ANIM_BOOMERANG);
    if (idleBlinkRepeatsLeft > 0)
    {
      idleBlinkRepeatsLeft--;
      scheduleNextIdleBlink(120, 220);
    }
    else
    {
      scheduleNextIdleBlink(3000, 7000);
    }
  }
}

// ============================================================================
// BOOT / DISPLAY UTILITIES
// ============================================================================
bool pressingCheck(String cmd, int ms)
{
  unsigned long start = millis();
  while (millis() - start < ms)
  {
    server.handleClient();
    dnsServer.processNextRequest();
    updateAnimatedFace();
    if (currentCommand != cmd)
    {
      runStandPose(1);
      return false;
    }
    yield();
  }
  return true;
}

/**
 * @brief Mark the time of the latest user input and dismiss the marquee.
 */
void recordInput()
{
  lastInputTime = millis();
  if (!firstInputReceived)
  {
    firstInputReceived = true;
    showingWifiInfo = false;
  }
}

/**
 * @brief Render the Wi-Fi connection-info marquee on the OLED.
 *
 * The marquee scrolls across the top row over the currently shown face
 * until the first user input arrives or the user explicitly dismisses it
 * via @ref recordInput.
 */
void updateWifiInfoScroll()
{
  // Don't show WiFi info if first input has been received
  if (firstInputReceived)
  {
    if (showingWifiInfo)
    {
      showingWifiInfo = false;
      // Restore the current face
      if (currentFaceFrames != nullptr && currentFaceFrameCount > 0)
      {
        updateFaceBitmap(currentFaceFrames[currentFaceFrameIndex]);
      }
    }
    return;
  }

  unsigned long now = millis();

  // Check if 30 seconds have passed without input
  if (!showingWifiInfo && (now - lastInputTime >= WIFI_MARQUEE_IDLE_MS))
  {
    showingWifiInfo = true;
    wifiScrollPos = 0;
    lastWifiScrollMs = now;
  }

  if (!showingWifiInfo)
    return;

  // Update scroll every 150ms
  if (now - lastWifiScrollMs >= WIFI_MARQUEE_TICK_MS)
  {
    lastWifiScrollMs = now;

    // Clear and redraw with current face in background
    display.clearDisplay();

    // Draw the face bitmap in the background
    if (currentFaceFrames != nullptr && currentFaceFrameCount > 0)
    {
      display.drawBitmap(0, 0, currentFaceFrames[currentFaceFrameIndex], 128, 64, SSD1306_WHITE);
    }

    // Draw black bar for text background on top row
    display.fillRect(0, 0, 128, 10, SSD1306_BLACK);

    // Draw scrolling text
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setTextWrap(false);
    display.setCursor(-wifiScrollPos, 1);
    display.print(wifiInfoText);
    display.setTextWrap(true);

    display.display();

    // Advance scroll position
    wifiScrollPos += WIFI_MARQUEE_SCROLL_PX;
    if (wifiScrollPos >= (int)(wifiInfoText.length() * 6))
    {
      wifiScrollPos = 0;
    }
  }
}
