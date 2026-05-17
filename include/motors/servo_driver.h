#pragma once

#include <Arduino.h>
#include "core/config.h"

// ============================================================================
// SERVO ADDRESSING
// ============================================================================

/**
 * @enum ServoName
 * @brief Symbolic identifiers for the eight robot servos.
 *
 * Numeric values map directly to indices in the servo array. The ordering
 * follows the physical wiring on the Lolin S2 Mini carrier and does NOT
 * match the lexicographic order of the labels.
 */
enum ServoName : uint8_t
{
  R1 = 0, ///< Hip, front right
  R2 = 1, ///< Hip, back right
  L1 = 2, ///< Hip, front left
  L2 = 3, ///< Hip, back left
  R4 = 4, ///< Foot, back right
  R3 = 5, ///< Foot, front right
  L3 = 6, ///< Foot, front left
  L4 = 7  ///< Foot, back left
};

/// Human-readable servo labels, ordered to match ServoName indices.
extern const String ServoNames[SERVO_COUNT];

/**
 * @brief Resolve a servo label (e.g. "R1", "L3") to its ServoName index.
 * @param servo Two-character canonical servo identifier.
 * @return The matching ServoName value, or -1 when the label is unknown.
 */
int servoNameToIndex(const String &servo);

// ============================================================================
// MOTORS NAMESPACE — hardware driver
// ============================================================================

namespace Motors
{
  /// Per-channel sub-trim in degrees, applied on top of every commanded angle.
  extern int8_t subtrim[SERVO_COUNT];

  /**
   * @brief Allocate PWM timers, attach all servos and move them to the
   *        neutral position one at a time to prevent brownout spikes.
   *
   * Must be called once from setup() after WiFi is up (to spread the
   * current draw over time relative to the AP bring-up).
   */
  void init();

  /**
   * @brief Drive a single servo to the requested angle with subtrim applied.
   *
   * Clamps the final angle to [0, 180] and calls delayWithFace() after
   * writing to spread current draw across consecutive writes.
   *
   * @param channel Servo index 0..SERVO_COUNT-1. Out-of-range calls ignored.
   * @param angle   Desired angle in degrees (0..180), before subtrim.
   */
  void setAngle(uint8_t channel, int angle);
} // namespace Motors
