#pragma once

#include <Arduino.h>
#include <IPAddress.h>

// ============================================================================
// WEB NAMESPACE
// ============================================================================

namespace Web
{
  /// `true` after the ESP32 successfully joined an upstream STA network.
  extern bool networkConnected;

  /// IP address obtained on the upstream LAN (only valid when networkConnected).
  extern IPAddress networkIP;

  /// mDNS hostname used for local network discovery.
  extern String deviceHostname;

  /**
   * @brief Register HTTP routes, start the DNS captive-portal trap, and
   *        bring the HTTP server online.
   *
   * Must be called once from setup() after the AP is up and all other
   * modules (Display, Motors) are initialised.
   *
   * @param apOk  `true` if WiFi.softAP() succeeded (DNS server only started
   *              when the AP is running).
   * @param apIP  Static AP IP to redirect DNS queries to (from AP_IP constant).
   */
  void init(bool apOk, IPAddress apIP);

  /**
   * @brief Per-loop service pump: drain the DNS queue and handle one HTTP
   *        request.
   *
   * Call this every iteration of loop() before the command dispatcher.
   */
  void pump();

} // namespace Web
