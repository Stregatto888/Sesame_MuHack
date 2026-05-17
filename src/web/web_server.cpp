// ============================================================================
// web/web_server.cpp — HTTP + DNS server, all route handlers
//
// Owns:
//   - WebServer (port 80) and DNSServer instances
//   - Hack-lock state (hackLocked / hackOwnerIP / hackOwnerMAC)
//   - Network connection state (networkConnected / networkIP / deviceHostname)
//
// Shared state consumed via extern (defined in main.cpp):
//   - String currentCommand  — command written by handlers, dispatched in loop()
//   - int    frameDelay, walkCycles, motorCurrentDelay — tunable timing params
// ============================================================================

#include "web/web_server.h"
#include "web/web_assets.h"
#include "core/config.h"
#include "display/face_engine.h"
#include "motors/servo_driver.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

// ---------------------------------------------------------------------------
// Shared state defined in main.cpp
// ---------------------------------------------------------------------------
extern String currentCommand;
extern int    frameDelay;
extern int    walkCycles;
extern int    motorCurrentDelay;

// ---------------------------------------------------------------------------
// Web namespace state
// ---------------------------------------------------------------------------
namespace Web
{
  bool      networkConnected = false;
  IPAddress networkIP;
  String    deviceHostname   = DEVICE_HOSTNAME;
} // namespace Web

// ---------------------------------------------------------------------------
// Module-private server instances
// ---------------------------------------------------------------------------
static WebServer  server(80);
static DNSServer  dnsServer;

// ---------------------------------------------------------------------------
// Hack-lock state (private to this module)
// ---------------------------------------------------------------------------
static bool      hackLocked   = false;
static IPAddress hackOwnerIP;
static String    hackOwnerMAC = "";

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

/**
 * @brief Check whether the current HTTP request is denied by the hack lock.
 *
 * When hackLocked is set, only the IP recorded in hackOwnerIP may drive the
 * robot.
 *
 * @return `true` if the request is from a non-owner client while locked.
 */
static bool isHackBlocked()
{
  if (!hackLocked)
    return false;
  IPAddress clientIP = server.client().remoteIP();
  return (clientIP != hackOwnerIP);
}

// ============================================================================
// HTTP HANDLERS
// ============================================================================

/**
 * @brief Serve the embedded captive-portal landing page.
 */
static void handleRoot()
{
  server.send(200, "text/html", index_html);
}

/**
 * @brief Handle text commands issued through the embedded terminal page.
 */
static void handleTerminalCmd()
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
    hackLocked   = true;
    hackOwnerIP  = clientIP;
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
      hackLocked  = false;
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
    Display::notifyInput();
    Display::exitIdle();
    server.send(200, "application/json", "{\"response\":\"Executing: " + cmdLower + "\"}");
    return;
  }

  if (cmdLower == "stop")
  {
    currentCommand = "";
    Display::notifyInput();
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
      Display::notifyInput();
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
 */
static void handleCommandWeb()
{
  if (isHackBlocked())
  {
    server.send(403, "text/plain", "LOCKED");
    return;
  }
  if (server.hasArg("pose"))
  {
    currentCommand = server.arg("pose");
    Display::notifyInput();
    Display::exitIdle();
    server.send(200, "text/plain", "OK");
  }
  else if (server.hasArg("go"))
  {
    currentCommand = server.arg("go");
    Display::notifyInput();
    Display::exitIdle();
    server.send(200, "text/plain", "OK");
  }
  else if (server.hasArg("stop"))
  {
    currentCommand = "";
    Display::notifyInput();
    server.send(200, "text/plain", "OK");
  }
  else if (server.hasArg("motor") && server.hasArg("value"))
  {
    int motorNum = server.arg("motor").toInt();
    int servoIdx = servoNameToIndex(server.arg("motor"));
    int angle    = server.arg("value").toInt();
    if (motorNum >= 1 && motorNum <= 8 && angle >= 0 && angle <= 180)
    {
      Motors::setAngle(motorNum - 1, angle);
      Display::notifyInput();
      server.send(200, "text/plain", "OK");
    }
    else if (servoIdx != -1 && angle >= 0 && angle <= 180)
    {
      Motors::setAngle(servoIdx, angle);
      Display::notifyInput();
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
static void handleGetSettings()
{
  String json = "{";
  json += "\"frameDelay\":"        + String(frameDelay)        + ",";
  json += "\"walkCycles\":"        + String(walkCycles)        + ",";
  json += "\"motorCurrentDelay\":" + String(motorCurrentDelay) + ",";
  json += "\"faceFps\":"           + String(Display::faceFps);
  json += "}";
  server.send(200, "application/json", json);
}

/**
 * @brief Apply tunable parameters received as URL-encoded form fields.
 */
static void handleSetSettings()
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
 * @brief Expose the runtime status as a JSON document.
 */
static void handleGetStatus()
{
  String json = "{";
  json += "\"currentCommand\":\"" + currentCommand + "\",";
  json += "\"currentFace\":\""    + Display::currentFaceName + "\",";
  json += "\"networkConnected\":" + String(Web::networkConnected ? "true" : "false") + ",";
  json += "\"apIP\":\""           + WiFi.softAPIP().toString() + "\"";
  if (Web::networkConnected)
  {
    json += ",\"networkIP\":\"" + Web::networkIP.toString() + "\"";
  }
  json += "}";
  server.send(200, "application/json", json);
}

/**
 * @brief JSON command endpoint for network-side clients (POST only).
 */
static void handleApiCommand()
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
    faceOnlyStart = body.indexOf("\"face\": \"");

  bool faceOnly = (faceOnlyStart > 0 &&
                   body.indexOf("\"command\":") == -1 &&
                   body.indexOf("\"command\": ") == -1);

  String command = "";
  String face    = "";

  // Parse face
  if (faceOnlyStart > 0)
  {
    faceOnlyStart = body.indexOf("\"", faceOnlyStart + 6) + 1;
    int faceEnd   = body.indexOf("\"", faceOnlyStart);
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
      cmdStart = body.indexOf("\"command\": \"");

    if (cmdStart == -1)
    {
      Serial.println("Error: command field not found");
      server.send(400, "application/json", "{\"error\":\"Missing command field\"}");
      return;
    }

    cmdStart   = body.indexOf("\"", cmdStart + 10) + 1;
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
    Display::set(face);

  // Face-only: just acknowledge
  if (faceOnly)
  {
    Display::notifyInput();
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Face updated\"}");
    return;
  }

  // Execute command
  if (command == "stop")
  {
    currentCommand = "";
    Display::notifyInput();
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Command stopped\"}");
  }
  else
  {
    currentCommand = command;
    Display::notifyInput();
    Display::exitIdle();
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Command executed\"}");
  }
}

// ============================================================================
// PUBLIC API
// ============================================================================

void Web::init(bool apOk, IPAddress apIP)
{
  // Start DNS captive-portal trap (redirect every hostname → apIP)
  if (apOk)
    dnsServer.start(DNS_PORT, "*", apIP);

  // Start mDNS responder for local network discovery
  if (MDNS.begin(Web::deviceHostname.c_str()))
  {
    Serial.println("mDNS responder started");
    MDNS.addService("http", "tcp", 80);
  }
  else
  {
    Serial.println("mDNS failed - not critical, continuing.");
  }

  // Register HTTP routes
  server.on("/",            handleRoot);
  server.on("/cmd",         handleCommandWeb);
  server.on("/getSettings", handleGetSettings);
  server.on("/setSettings", handleSetSettings);
  server.on("/terminal",    handleTerminalCmd);
  server.on("/api/status",  handleGetStatus);
  server.on("/api/command", handleApiCommand);
  server.onNotFound(handleRoot);
  server.begin();
}

void Web::pump()
{
  dnsServer.processNextRequest();
  server.handleClient();
}
