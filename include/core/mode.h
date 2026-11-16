#pragma once

#include <stdint.h>

// ============================================================================
// MODE — who drives the servos (fase 5)
// ============================================================================
//
//   WEB (boot default) — web UI, terminal and serial CLI run poses and gaits.
//   ROS                — only the ROS bridge (TaskRos) writes the servos.
//
// The robot enters ROS mode on the first valid ROS command and stays there
// (holding its pose when the 250 ms watchdog fires) until the web terminal
// sends "mode web" with ROS idle for at least ROS_IDLE_FOR_WEB_MS, or reboot.
// ============================================================================

namespace Mode
{
  /// `true` while ROS 2 owns the servos.
  bool isRos();

  /// Called by TaskRos on every valid command: enters ROS mode and stamps the time.
  void noteRosCommand();

  /**
   * @brief Ask to go back to WEB mode.
   * @return `true` if the robot is now in WEB mode; `false` if ROS commands
   *         arrived during the last ROS_IDLE_FOR_WEB_MS milliseconds.
   */
  bool requestWeb();
} // namespace Mode
