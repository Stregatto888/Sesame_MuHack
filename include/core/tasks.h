#pragma once

#include <Arduino.h>

// ============================================================================
// TASKS
// ============================================================================
//
// Three FreeRTOS tasks that replace the cooperative loop():
//
//   TaskWeb     (priority 1) — Web::pump() at ~1 ms cadence
//   TaskDisplay (priority 1) — Display::tick*() at ~10 ms cadence
//   TaskMotor   (priority 2) — CmdQueue::pop() + command dispatcher + serial CLI
//
// Call Tasks::startAll() at the end of setup(). After that, loop() should
// yield indefinitely with vTaskDelay(portMAX_DELAY).
// ============================================================================

namespace Tasks
{
  /**
   * @brief Create and start all three FreeRTOS tasks.
   *
   * Must be called once from setup() after CmdQueue::init() and all hardware
   * modules (Web::init, Motors::init, Display::init) are ready.
   */
  void startAll();

} // namespace Tasks

// ============================================================================
// COOPERATIVE TIMING HELPERS
// ============================================================================
//
// Called by pose / gait routines (in poses.cpp) and the servo driver
// (servo_driver.cpp). Implemented in tasks.cpp using vTaskDelay so that the
// other tasks (TaskWeb, TaskDisplay) continue to run during long poses.
// ============================================================================

/**
 * @brief Yield for `ms` milliseconds, allowing other tasks to run.
 *
 * Replaces the old cooperative spin that manually called Web::pump() and
 * Display::tickFace(). Those are now handled by their own tasks.
 */
void delayWithFace(unsigned long ms);

/**
 * @brief Wait up to `ms` milliseconds; abort early if the active command changes.
 *
 * Checks the CmdQueue for a new command each 5 ms. If a new command is found
 * the robot stands up and the function returns `false`, signalling the caller
 * to abort the current gait/pose.
 *
 * @param cmd  The command that must still be active for the wait to continue.
 * @param ms   Maximum wait time (milliseconds).
 * @return `true` if the full wait elapsed without interruption.
 */
bool pressingCheck(String cmd, int ms);
