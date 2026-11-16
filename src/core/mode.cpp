#include "core/mode.h"
#include "core/config.h"

#include <atomic>
#include "esp_timer.h"

namespace
{
std::atomic<bool>     rosActive{false};
std::atomic<uint32_t> lastRosCommandMs{0};

uint32_t nowMs()
{
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}
} // namespace

bool Mode::isRos()
{
  return rosActive.load();
}

void Mode::noteRosCommand()
{
  lastRosCommandMs.store(nowMs());
  rosActive.store(true);
}

bool Mode::requestWeb()
{
  if (!rosActive.load())
    return true;
  if (nowMs() - lastRosCommandMs.load() < ROS_IDLE_FOR_WEB_MS)
    return false;
  rosActive.store(false);
  return true;
}
