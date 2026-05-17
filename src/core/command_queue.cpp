// ============================================================================
// core/command_queue.cpp — FreeRTOS queue for motor commands
//
// Single queue, char-array items (avoids heap allocation inside ISR context).
// ============================================================================

#include "core/command_queue.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// ---------------------------------------------------------------------------
// Module-private state
// ---------------------------------------------------------------------------

/// Raw FreeRTOS queue handle (char[CMD_MAX_LEN] items).
static QueueHandle_t s_queue = nullptr;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void CmdQueue::init()
{
  s_queue = xQueueCreate(CMD_QUEUE_DEPTH, CMD_MAX_LEN * sizeof(char));
  configASSERT(s_queue != nullptr);
}

void CmdQueue::push(const String &cmd)
{
  configASSERT(s_queue != nullptr);

  char buf[CMD_MAX_LEN];
  cmd.toCharArray(buf, sizeof(buf));

  // If the queue is full, drain one item to make room (last-write-wins).
  if (uxQueueSpacesAvailable(s_queue) == 0)
  {
    char discard[CMD_MAX_LEN];
    xQueueReceive(s_queue, discard, 0);
  }

  xQueueSendToBack(s_queue, buf, 0);
}

bool CmdQueue::pop(String &cmd)
{
  configASSERT(s_queue != nullptr);

  char buf[CMD_MAX_LEN];
  if (xQueueReceive(s_queue, buf, 0) == pdTRUE)
  {
    cmd = String(buf);
    return true;
  }
  return false;
}
