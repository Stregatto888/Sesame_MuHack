#include "motors/servo_driver.h"
#include "core/tasks.h"

#include <ESP32Servo.h>

// motorCurrentDelay remains a global in main.cpp (Phase 6 will protect it).
extern int motorCurrentDelay;

// ============================================================================
// SERVO LABELS
// ============================================================================

const String ServoNames[SERVO_COUNT] = {"R1", "R2", "L1", "L2", "R4", "R3", "L3", "L4"};

int servoNameToIndex(const String &servo)
{
  if (servo == "L1") return L1;
  if (servo == "L2") return L2;
  if (servo == "L3") return L3;
  if (servo == "L4") return L4;
  if (servo == "R1") return R1;
  if (servo == "R2") return R2;
  if (servo == "R3") return R3;
  if (servo == "R4") return R4;
  return -1;
}

// ============================================================================
// MOTORS NAMESPACE — state
// ============================================================================

namespace Motors
{

int8_t subtrim[SERVO_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0};

/// Servo objects — file-local, accessed only through Motors::setAngle().
static Servo servos[SERVO_COUNT];

// ============================================================================
// MOTORS NAMESPACE — implementation
// ============================================================================

void init()
{
  // Allocate all four ESP32 PWM hardware timers required by ESP32Servo.
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // Attach and initialise servos ONE AT A TIME to prevent brownout.
  // Each servo draws a current spike when energised; staggering the attaches
  // by SERVO_ATTACH_STAGGER_MS keeps the peak below the AP power budget.
  for (int i = 0; i < SERVO_COUNT; i++)
  {
    servos[i].setPeriodHertz(SERVO_PWM_HZ);
    servos[i].attach(SERVO_PINS[i], SERVO_PULSE_MIN_US, SERVO_PULSE_MAX_US);
    servos[i].write(SERVO_INIT_ANGLE);
    delay(SERVO_ATTACH_STAGGER_MS);
  }
}

void setAngle(uint8_t channel, int angle)
{
  if (channel >= SERVO_COUNT)
    return;

  int adjustedAngle = constrain(angle + subtrim[channel], 0, 180);

  Serial.print(F("[DEBUG] Motors::setAngle: ch="));
  Serial.print(channel);
  Serial.print(F(" ("));
  Serial.print(ServoNames[channel]);
  Serial.print(F(") -> angle="));
  Serial.println(adjustedAngle);

  servos[channel].write(adjustedAngle);
  delayWithFace(motorCurrentDelay); // Phase 5: replace with vTaskDelay
}

} // namespace Motors
