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
#include "captive-portal.h"
#include "core/config.h"
#include "display/face_engine.h"
#include "motors/servo_driver.h"
#include "motors/poses.h"

// ============================================================================
// PERIPHERAL INSTANCES
// ============================================================================

/// DNS server intercepting every query so the captive portal auto-pops up.
DNSServer dnsServer;

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
// COMMAND DISPATCHER STATE
// ============================================================================

String currentCommand = "";                              ///< Active command from any UI.

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
// FORWARD DECLARATIONS
// ============================================================================

void delayWithFace(unsigned long ms);
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
    resp += "\\nFace: " + Display::currentFaceName;
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
    Display::exitIdle();
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
      Display::exitIdle();
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
    Display::exitIdle();
    server.send(200, "text/plain", "OK");
  }
  else if (server.hasArg("go"))
  {
    currentCommand = server.arg("go");
    recordInput();
    Display::exitIdle();
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
  json += "\"faceFps\":" + String(Display::faceFps);
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
    Display::faceFps = (int)max(1L, server.arg("faceFps").toInt());
  server.send(200, "text/plain", "OK");
}

/**
 * @brief Expose the runtime status as a JSON document for remote clients.
 */
void handleGetStatus()
{
  String json = "{";
  json += "\"currentCommand\":\"" + currentCommand + "\",";
  json += "\"currentFace\":\"" + Display::currentFaceName + "\",";
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
    Display::set(face);
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
    Display::exitIdle();
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Command executed\"}");
  }
}

// ============================================================================
// BOOT / DISPLAY UTILITIES
// ============================================================================\n\n// ============================================================================
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

  // OLED Init (non-blocking: robot works even without display).
  // Wire.begin() was called above; Display::init() starts the SSD1306.
  bool displayOk = Display::init();
  if (displayOk) Display::bootMsg("Sesame MuHack Boot");

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
    Display::bootMsg("Starting AP...");

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
      Display::bootMsg(("AP OK: " + String(AP_SSID)).c_str());
  }
  else
  {
    Serial.println("WARNING: AP failed, retrying...");
    if (displayOk)
      Display::bootMsg("AP FAILED! Retrying...");
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
        Display::bootMsg("AP OK (retry)");
    }
    else
    {
      Serial.println("AP creation failed permanently. Robot will work offline.");
      if (displayOk)
        Display::bootMsg("AP FAILED - offline mode");
    }
  }

  // === OPTIONAL: Station mode (non-blocking) ===
  if (ENABLE_NETWORK_MODE && String(NETWORK_SSID).length() > 0)
  {
    if (displayOk)
      Display::bootMsg("Connecting WiFi...");
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
        Display::bootMsg(("Net: " + networkIP.toString()).c_str());
    }
    else
    {
      Serial.println();
      Serial.println("Network connection failed - AP still active, continuing.");
      if (displayOk)
        Display::bootMsg("Net: skip (AP only)");
      WiFi.disconnect(false); // Stop STA scanning, keep AP
    }
  }
  else
  {
    Serial.println("Network mode disabled. Running in AP-only mode.");
  }

  // Build WiFi info text and hand it to the display module.
  // myIP is already set to the static AP_IP — no need to call softAPIP().
  String wifiInfoText;
  if (networkConnected)
  {
    wifiInfoText = "AP: " + String(AP_SSID) + " (" + myIP.toString() + ")  |  Network: " + String(NETWORK_SSID) + " (" + networkIP.toString() + ") or " + deviceHostname + ".local  |  ";
  }
  else
  {
    wifiInfoText = "Connect to WiFi: " + String(AP_SSID) + "  |  Pass: " + String(AP_PASS) + "  |  IP: " + myIP.toString() + "  |  Captive Portal will auto-open!  |  ";
  }
  Display::setMarqueeText(wifiInfoText);

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
    Display::bootMsg("Init servos...");
  Motors::init();

  // Show rest face on startup
  if (displayOk)
    Display::set("rest");

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
  Display::tickFace();
  Display::tickIdle();
  Display::tickMarquee();

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
// COOPERATIVE HELPERS
// ============================================================================

/**
 * @brief Cooperative delay: keeps DNS, HTTP and face animation alive.
 *
 * Pose / gait routines call this instead of delay() so the robot remains
 * responsive while a limb is in motion.
 * Temporary: moves into a Motors/scheduler context in Phase 5.
 */
void delayWithFace(unsigned long ms)
{
  unsigned long start = millis();
  while (millis() - start < ms)
  {
    Display::tickFace();
    server.handleClient();
    dnsServer.processNextRequest();
    delay(5);
  }
}

bool pressingCheck(String cmd, int ms)
{
  unsigned long start = millis();
  while (millis() - start < ms)
  {
    server.handleClient();
    dnsServer.processNextRequest();
    Display::tickFace();
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
  Display::notifyInput();
}
