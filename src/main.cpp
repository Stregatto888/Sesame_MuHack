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
#include <Wire.h>
#include "core/config.h"
#include "core/command_queue.h"
#include "core/tasks.h"
#include "display/face_engine.h"
#include "motors/servo_driver.h"
#include "motors/poses.h"
#include "web/web_server.h"

// ============================================================================
// PERIPHERAL INSTANCES
// ============================================================================

// Servo objects, pin mapping and subtrim values are owned by the
// Motors namespace in src/motors/servo_driver.cpp.
// Access subtrim via Motors::subtrim[]  and servo writes via Motors::setAngle().

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

// Network and hack-lock state is owned by Web namespace (src/web/web_server.cpp).

// HTTP handlers are in src/web/web_server.cpp (Web namespace).
// delayWithFace() and pressingCheck() are in src/core/tasks.cpp.

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
      Web::networkConnected = true;
      Web::networkIP = WiFi.localIP();
      Serial.println();
      Serial.print("Connected to network! IP: ");
      Serial.println(Web::networkIP);
      if (displayOk)
        Display::bootMsg(("Net: " + Web::networkIP.toString()).c_str());
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
  if (Web::networkConnected)
  {
    wifiInfoText = "AP: " + String(AP_SSID) + " (" + myIP.toString() + ")  |  Network: " + String(NETWORK_SSID) + " (" + Web::networkIP.toString() + ") or " + Web::deviceHostname + ".local  |  ";
  }
  else
  {
    wifiInfoText = "Connect to WiFi: " + String(AP_SSID) + "  |  Pass: " + String(AP_PASS) + "  |  IP: " + myIP.toString() + "  |  Captive Portal will auto-open!  |  ";
  }
  Display::setMarqueeText(wifiInfoText);

  // Start DNS captive-portal trap, mDNS, and HTTP server
  Web::init(apOk, myIP);

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

  // Initialise command queue then start the three FreeRTOS tasks.
  // CmdQueue::init() must precede Tasks::startAll() so the queue handle
  // exists before TaskMotor starts popping.
  CmdQueue::init();
  Tasks::startAll();
}

/**
 * @brief Arduino loop — yields forever; all work is done in FreeRTOS tasks.
 *
 * TaskWeb, TaskDisplay, and TaskMotor are started in setup() via
 * Tasks::startAll(). The Arduino loopTask suspends itself here so it does
 * not waste CPU cycles.
 */
void loop()
{
  vTaskDelay(portMAX_DELAY);
}
