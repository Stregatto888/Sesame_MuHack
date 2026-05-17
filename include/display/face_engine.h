#pragma once

#include <Arduino.h>

// ============================================================================
// FACE ANIMATION MODE
// ============================================================================

/**
 * @enum FaceAnimMode
 * @brief Playback strategy for OLED face animation sequences.
 *
 * Consumed by Display::setMode / Display::setWithMode to drive the
 * per-frame state machine inside the display module.
 *
 * Moved here from include/motors/servo_driver.h in Phase 2.
 */
enum FaceAnimMode : uint8_t
{
  FACE_ANIM_LOOP = 0,     ///< Cycle frames forever (e.g. ambient idle).
  FACE_ANIM_ONCE = 1,     ///< Play once and freeze on the last frame.
  FACE_ANIM_BOOMERANG = 2 ///< Ping-pong between first and last frame.
};

// ============================================================================
// DISPLAY NAMESPACE
// ============================================================================

namespace Display
{
  /// Symbolic name of the face currently shown on the OLED.
  /// Read-only from outside the module; written only by Display::set().
  extern String currentFaceName;

  /// Global FPS fallback for faces not listed in the per-face override table.
  /// Writable from outside to allow runtime tuning via /setSettings.
  extern int faceFps;

  /**
   * @brief Initialise the SSD1306 hardware (clear, set text defaults).
   *
   * Must be called from setup() after Wire.begin().
   * Initialises the idle-timer so the marquee won't appear until
   * Display::setMarqueeText() has been called.
   *
   * @return true if the display was found and initialised successfully.
   */
  bool init();

  /**
   * @brief Store the pre-built WiFi info string for the scrolling marquee
   *        and reset the idle timer.
   *
   * Call this once in setup() after the AP/STA configuration is complete
   * and the wifiInfoText string has been assembled.
   *
   * @param text Scrolling marquee text (AP name, IP, etc.).
   */
  void setMarqueeText(const String &text);

  /**
   * @brief Print a message on the OLED and serial console.
   * @param msg   Null-terminated text to display.
   * @param clear When true, clear the screen before writing.
   */
  void bootMsg(const char *msg, bool clear = false);

  /** Switch to the named face and reset animation state. */
  void set(const String &faceName);

  /** Override the playback mode without changing the displayed face. */
  void setMode(FaceAnimMode mode);

  /** Convenience: set mode then switch face atomically. */
  void setWithMode(const String &faceName, FaceAnimMode mode);

  /** Advance the face animation by one tick. Call every loop iteration. */
  void tickFace();

  /** Drive the idle-blink state machine. Call every loop iteration. */
  void tickIdle();

  /** Drive the WiFi info marquee. Call every loop iteration. */
  void tickMarquee();

  /** Enter the idle face + random-blink scheduler. */
  void enterIdle();

  /** Leave idle state without altering the current face. */
  void exitIdle();

  /**
   * @brief Record a user-input event.
   *
   * Resets the marquee idle timer and permanently dismisses the scrolling
   * banner after the first call.
   */
  void notifyInput();

} // namespace Display
