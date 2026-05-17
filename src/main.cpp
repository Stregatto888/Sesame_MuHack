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

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void delayWithFace(unsigned long ms);
bool pressingCheck(String cmd, int ms);
void recordInput();

// HTTP handlers are now in src/web/web_server.cpp (Web namespace).

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
  // Drain DNS captive-portal queue and handle HTTP requests
  Web::pump();
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
    Web::pump();
    delay(5);
  }
}

bool pressingCheck(String cmd, int ms)
{
  unsigned long start = millis();
  while (millis() - start < ms)
  {
    Web::pump();
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
