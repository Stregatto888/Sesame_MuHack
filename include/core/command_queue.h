#pragma once

#include <Arduino.h>

// ============================================================================
// COMMAND QUEUE
// ============================================================================
//
// Thin FreeRTOS queue wrapper that decouples the web/serial producers from the
// motor dispatcher consumer.
//
//  Producers  (Web handlers, serial CLI)  →  CmdQueue::push()
//  Consumer   (loop() dispatcher)         →  CmdQueue::pop()
//
// The queue holds at most CMD_QUEUE_DEPTH items. If the queue is full, push()
// silently drops the oldest entry and inserts the new one (last-write-wins),
// which mirrors the original `currentCommand = cmd` semantics.
// ============================================================================

namespace CmdQueue
{
  /// Maximum number of pending commands. Keep this small — only the latest
  /// command matters; the motor dispatcher always drains the queue fully.
  static constexpr UBaseType_t CMD_QUEUE_DEPTH = 4;

  /// Maximum length of a command string (including null terminator).
  static constexpr size_t CMD_MAX_LEN = 32;

  /**
   * @brief Create the FreeRTOS queue. Must be called once from setup() before
   *        any push() or pop().
   */
  void init();

  /**
   * @brief Enqueue a command string.
   *
   * Thread-safe: safe to call from HTTP handler callbacks.
   * If the queue is full, the oldest item is discarded so the new command
   * is never lost.
   *
   * @param cmd  Command name (e.g. "forward", "wave", "stop", "").
   */
  void push(const String &cmd);

  /**
   * @brief Dequeue the next command, if any.
   *
   * Non-blocking (timeout = 0). Returns `false` immediately if the queue is
   * empty.
   *
   * @param[out] cmd  Receives the command string when returning `true`.
   * @return `true` if a command was available.
   */
  bool pop(String &cmd);

} // namespace CmdQueue
