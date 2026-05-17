#include "motors/poses.h"
#include "motors/servo_driver.h"
#include "display/face_engine.h"

// ============================================================================
// TEMPORARY PHASE-1/2 DEPENDENCIES (still defined in main.cpp)
// These externs will be replaced by FreeRTOS queue reads in Phase 4/5.
// ============================================================================

extern int frameDelay;        ///< Inter-frame pause used by gait routines (ms).
extern int walkCycles;        ///< Repetitions per locomotion gait invocation.
extern String currentCommand; ///< Active command string — will become a queue in Phase 4.

extern void delayWithFace(unsigned long ms);
extern bool pressingCheck(String cmd, int ms);

// Convenience alias so pose bodies read naturally.
// Motors::setAngle is the canonical call; this alias is removed in Phase 5.
static inline void setServoAngle(uint8_t ch, int angle) { Motors::setAngle(ch, angle); }

// ============================================================================
// STATIC POSES
// ============================================================================

void runRestPose()
{
  Serial.println(F("REST"));
  Display::setWithMode("rest", FACE_ANIM_BOOMERANG);
  for (int i = 0; i < SERVO_COUNT; i++)
    Motors::setAngle(i, 90);
}

void runStandPose(int face)
{
  Serial.println(F("STAND"));
  Serial.print(F("[DEBUG] runStandPose: entering stand pose, face="));
  Serial.println(face);
  if (face == 1)
    Display::setWithMode("stand", FACE_ANIM_ONCE);
  setServoAngle(R1, 135);
  setServoAngle(R2, 45);
  setServoAngle(L1, 45);
  setServoAngle(L2, 135);
  setServoAngle(R4, 0);
  setServoAngle(R3, 180);
  setServoAngle(L3, 0);
  setServoAngle(L4, 180);
  if (face == 1)
    Display::enterIdle();
}

void runWavePose()
{
  Serial.println(F("WAVE"));
  Display::setWithMode("wave", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  setServoAngle(R4, 80);
  setServoAngle(L3, 180);
  setServoAngle(L2, 60);
  setServoAngle(R1, 100);
  delayWithFace(200);
  setServoAngle(L3, 180);
  delayWithFace(300);
  for (int i = 0; i < 4; i++)
  {
    setServoAngle(L3, 180);
    delayWithFace(300);
    setServoAngle(L3, 100);
    delayWithFace(300);
  }
  runStandPose(1);
  if (currentCommand == "wave")
    currentCommand = "";
}

void runDancePose()
{
  Serial.println(F("DANCE"));
  Display::setWithMode("dance", FACE_ANIM_LOOP);
  setServoAngle(R1, 90);
  setServoAngle(R2, 90);
  setServoAngle(L1, 90);
  setServoAngle(L2, 90);
  setServoAngle(R4, 160);
  setServoAngle(R3, 160);
  setServoAngle(L3, 10);
  setServoAngle(L4, 10);
  delayWithFace(300);
  for (int i = 0; i < 5; i++)
  {
    setServoAngle(R4, 115);
    setServoAngle(R3, 115);
    setServoAngle(L3, 10);
    setServoAngle(L4, 10);
    delayWithFace(300);
    setServoAngle(R4, 160);
    setServoAngle(R3, 160);
    setServoAngle(L3, 65);
    setServoAngle(L4, 65);
    delayWithFace(300);
  }
  runStandPose(1);
  if (currentCommand == "dance")
    currentCommand = "";
}

void runSwimPose()
{
  Serial.println(F("SWIM"));
  Display::setWithMode("swim", FACE_ANIM_ONCE);
  for (int i = 0; i < SERVO_COUNT; i++)
    Motors::setAngle(i, 90);
  for (int i = 0; i < 4; i++)
  {
    setServoAngle(R1, 135);
    setServoAngle(R2, 45);
    setServoAngle(L1, 45);
    setServoAngle(L2, 135);
    delayWithFace(400);
    setServoAngle(R1, 90);
    setServoAngle(R2, 90);
    setServoAngle(L1, 90);
    setServoAngle(L2, 90);
    delayWithFace(400);
  }
  runStandPose(1);
  if (currentCommand == "swim")
    currentCommand = "";
}

void runPointPose()
{
  Serial.println(F("POINT"));
  Display::setWithMode("point", FACE_ANIM_BOOMERANG);
  setServoAngle(L2, 60);
  setServoAngle(R1, 135);
  setServoAngle(R2, 100);
  setServoAngle(L4, 180);
  setServoAngle(L1, 25);
  setServoAngle(L3, 145);
  setServoAngle(R4, 80);
  setServoAngle(R3, 170);
  delayWithFace(2000);
  runStandPose(1);
  if (currentCommand == "point")
    currentCommand = "";
}

void runPushupPose()
{
  Serial.println(F("PUSHUP"));
  Display::setWithMode("pushup", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  setServoAngle(L1, 0);
  setServoAngle(R1, 180);
  setServoAngle(L3, 90);
  setServoAngle(R3, 90);
  delayWithFace(500);
  for (int i = 0; i < 4; i++)
  {
    setServoAngle(L3, 0);
    setServoAngle(R3, 180);
    delayWithFace(600);
    setServoAngle(L3, 90);
    setServoAngle(R3, 90);
    delayWithFace(500);
  }
  runStandPose(1);
  if (currentCommand == "pushup")
    currentCommand = "";
}

void runBowPose()
{
  Serial.println(F("BOW"));
  Display::setWithMode("bow", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  setServoAngle(L1, 0);
  setServoAngle(R1, 180);
  setServoAngle(L3, 0);
  setServoAngle(R3, 180);
  setServoAngle(L2, 180);
  setServoAngle(R2, 0);
  setServoAngle(R4, 0);
  setServoAngle(L4, 180);
  delayWithFace(600);
  setServoAngle(L3, 90);
  setServoAngle(R3, 90);
  delayWithFace(3000);
  runStandPose(1);
  if (currentCommand == "bow")
    currentCommand = "";
}

void runCutePose()
{
  Serial.println(F("CUTE"));
  Display::setWithMode("cute", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  setServoAngle(L2, 160);
  setServoAngle(R2, 20);
  setServoAngle(R4, 180);
  setServoAngle(L4, 0);
  setServoAngle(L1, 0);
  setServoAngle(R1, 180);
  setServoAngle(L3, 180);
  setServoAngle(R3, 0);
  delayWithFace(200);
  for (int i = 0; i < 5; i++)
  {
    setServoAngle(R4, 180);
    setServoAngle(L4, 45);
    delayWithFace(300);
    setServoAngle(R4, 135);
    setServoAngle(L4, 0);
    delayWithFace(300);
  }
  runStandPose(1);
  if (currentCommand == "cute")
    currentCommand = "";
}

void runFreakyPose()
{
  Serial.println(F("FREAKY"));
  Display::setWithMode("freaky", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  setServoAngle(L1, 0);
  setServoAngle(R1, 180);
  setServoAngle(L2, 180);
  setServoAngle(R2, 0);
  setServoAngle(R4, 90);
  setServoAngle(R3, 0);
  delayWithFace(200);
  for (int i = 0; i < 3; i++)
  {
    setServoAngle(R3, 25);
    delayWithFace(400);
    setServoAngle(R3, 0);
    delayWithFace(400);
  }
  runStandPose(1);
  if (currentCommand == "freaky")
    currentCommand = "";
}

void runWormPose()
{
  Serial.println(F("WORM"));
  Display::setWithMode("worm", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  setServoAngle(R1, 180);
  setServoAngle(R2, 0);
  setServoAngle(L1, 0);
  setServoAngle(L2, 180);
  setServoAngle(R4, 90);
  setServoAngle(R3, 90);
  setServoAngle(L3, 90);
  setServoAngle(L4, 90);
  delayWithFace(200);
  for (int i = 0; i < 5; i++)
  {
    setServoAngle(R3, 45);
    setServoAngle(L3, 135);
    setServoAngle(R4, 45);
    setServoAngle(L4, 135);
    delayWithFace(300);
    setServoAngle(R3, 135);
    setServoAngle(L3, 45);
    setServoAngle(R4, 135);
    setServoAngle(L4, 45);
    delayWithFace(300);
  }
  runStandPose(1);
  if (currentCommand == "worm")
    currentCommand = "";
}

void runShakePose()
{
  Serial.println(F("SHAKE"));
  Display::setWithMode("shake", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  setServoAngle(R1, 135);
  setServoAngle(L1, 45);
  setServoAngle(L3, 90);
  setServoAngle(R3, 90);
  setServoAngle(L2, 90);
  setServoAngle(R2, 90);
  delayWithFace(200);
  for (int i = 0; i < 5; i++)
  {
    setServoAngle(R4, 45);
    setServoAngle(L4, 135);
    delayWithFace(300);
    setServoAngle(R4, 0);
    setServoAngle(L4, 180);
    delayWithFace(300);
  }
  runStandPose(1);
  if (currentCommand == "shake")
    currentCommand = "";
}

void runShrugPose()
{
  Serial.println(F("SHRUG"));
  runStandPose(0);
  Display::setWithMode("dead", FACE_ANIM_ONCE);
  delayWithFace(200);
  setServoAngle(R3, 90);
  setServoAngle(R4, 90);
  setServoAngle(L3, 90);
  setServoAngle(L4, 90);
  delayWithFace(1000);
  Display::setWithMode("shrug", FACE_ANIM_ONCE);
  setServoAngle(R3, 0);
  setServoAngle(R4, 180);
  setServoAngle(L3, 180);
  setServoAngle(L4, 0);
  delayWithFace(1500);
  runStandPose(1);
  if (currentCommand == "shrug")
    currentCommand = "";
}

void runDeadPose()
{
  Serial.println(F("DEAD"));
  runStandPose(0);
  Display::setWithMode("dead", FACE_ANIM_BOOMERANG);
  delayWithFace(200);
  setServoAngle(R3, 90);
  setServoAngle(R4, 90);
  setServoAngle(L3, 90);
  setServoAngle(L4, 90);
  if (currentCommand == "dead")
    currentCommand = "";
}

void runCrabPose()
{
  Serial.println(F("CRAB"));
  Display::setWithMode("crab", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  setServoAngle(R1, 90);
  setServoAngle(R2, 90);
  setServoAngle(L1, 90);
  setServoAngle(L2, 90);
  setServoAngle(R4, 0);
  setServoAngle(R3, 180);
  setServoAngle(L3, 45);
  setServoAngle(L4, 135);
  for (int i = 0; i < 5; i++)
  {
    setServoAngle(R4, 45);
    setServoAngle(R3, 135);
    setServoAngle(L3, 0);
    setServoAngle(L4, 180);
    delayWithFace(300);
    setServoAngle(R4, 0);
    setServoAngle(R3, 180);
    setServoAngle(L3, 45);
    setServoAngle(L4, 135);
    delayWithFace(300);
  }
  runStandPose(1);
  if (currentCommand == "crab")
    currentCommand = "";
}

// ============================================================================
// LOCOMOTION GAITS
// ============================================================================

void runWalkPose()
{
  Serial.println(F("WALK FWD"));
  Display::setWithMode("walk", FACE_ANIM_ONCE);
  // Initial step
  setServoAngle(R3, 135);
  setServoAngle(L3, 45);
  setServoAngle(R2, 100);
  setServoAngle(L1, 25);
  if (!pressingCheck("forward", frameDelay))
    return;

  for (int i = 0; i < walkCycles; i++)
  {
    setServoAngle(R3, 135);
    setServoAngle(L3, 0);
    if (!pressingCheck("forward", frameDelay))
      return;
    setServoAngle(L4, 135);
    setServoAngle(L2, 90);
    setServoAngle(R4, 0);
    setServoAngle(R1, 180);
    if (!pressingCheck("forward", frameDelay))
      return;
    setServoAngle(R2, 45);
    setServoAngle(L1, 90);
    if (!pressingCheck("forward", frameDelay))
      return;
    setServoAngle(R4, 45);
    setServoAngle(L4, 180);
    if (!pressingCheck("forward", frameDelay))
      return;
    setServoAngle(R3, 180);
    setServoAngle(L3, 45);
    setServoAngle(R2, 90);
    setServoAngle(L1, 0);
    if (!pressingCheck("forward", frameDelay))
      return;
    setServoAngle(L2, 135);
    setServoAngle(R1, 90);
    if (!pressingCheck("forward", frameDelay))
      return;
  }
  runStandPose(1);
}

void runWalkBackward()
{
  Serial.println(F("WALK BACK"));
  Display::setWithMode("walk", FACE_ANIM_ONCE);
  if (!pressingCheck("backward", frameDelay))
    return;

  for (int i = 0; i < walkCycles; i++)
  {
    setServoAngle(R3, 135);
    setServoAngle(L3, 0);
    if (!pressingCheck("backward", frameDelay))
      return;
    setServoAngle(L4, 135);
    setServoAngle(L2, 135);
    setServoAngle(R4, 0);
    setServoAngle(R1, 90);
    if (!pressingCheck("backward", frameDelay))
      return;
    setServoAngle(R2, 90);
    setServoAngle(L1, 0);
    if (!pressingCheck("backward", frameDelay))
      return;
    setServoAngle(R4, 45);
    setServoAngle(L4, 180);
    if (!pressingCheck("backward", frameDelay))
      return;
    setServoAngle(R3, 180);
    setServoAngle(L3, 45);
    setServoAngle(R2, 45);
    setServoAngle(L1, 90);
    if (!pressingCheck("backward", frameDelay))
      return;
    setServoAngle(L2, 90);
    setServoAngle(R1, 180);
    if (!pressingCheck("backward", frameDelay))
      return;
  }
  runStandPose(1);
}

void runTurnLeft()
{
  Serial.println(F("TURN LEFT"));
  Display::setWithMode("walk", FACE_ANIM_ONCE);
  for (int i = 0; i < walkCycles; i++)
  {
    // legset 1 (R1 L2)
    setServoAngle(R3, 135);
    setServoAngle(L4, 135);
    if (!pressingCheck("left", frameDelay))
      return;
    setServoAngle(R1, 180);
    setServoAngle(L2, 180);
    if (!pressingCheck("left", frameDelay))
      return;
    setServoAngle(R3, 180);
    setServoAngle(L4, 180);
    if (!pressingCheck("left", frameDelay))
      return;
    setServoAngle(R1, 135);
    setServoAngle(L2, 135);
    if (!pressingCheck("left", frameDelay))
      return;
    // legset 2 (R2 L1)
    setServoAngle(R4, 45);
    setServoAngle(L3, 45);
    if (!pressingCheck("left", frameDelay))
      return;
    setServoAngle(R2, 90);
    setServoAngle(L1, 90);
    if (!pressingCheck("left", frameDelay))
      return;
    setServoAngle(R4, 0);
    setServoAngle(L3, 0);
    if (!pressingCheck("left", frameDelay))
      return;
    setServoAngle(R2, 45);
    setServoAngle(L1, 45);
    if (!pressingCheck("left", frameDelay))
      return;
  }
  runStandPose(1);
}

void runTurnRight()
{
  Serial.println(F("TURN RIGHT"));
  Display::setWithMode("walk", FACE_ANIM_ONCE);
  for (int i = 0; i < walkCycles; i++)
  {
    // legset 2 (R2 L1)
    setServoAngle(R4, 45);
    setServoAngle(L3, 45);
    if (!pressingCheck("right", frameDelay))
      return;
    setServoAngle(R2, 0);
    setServoAngle(L1, 0);
    if (!pressingCheck("right", frameDelay))
      return;
    setServoAngle(R4, 0);
    setServoAngle(L3, 0);
    if (!pressingCheck("right", frameDelay))
      return;
    setServoAngle(R2, 45);
    setServoAngle(L1, 45);
    if (!pressingCheck("right", frameDelay))
      return;
    // legset 1 (R1 L2)
    setServoAngle(R3, 135);
    setServoAngle(L4, 135);
    if (!pressingCheck("right", frameDelay))
      return;
    setServoAngle(R1, 90);
    setServoAngle(L2, 90);
    if (!pressingCheck("right", frameDelay))
      return;
    setServoAngle(R3, 180);
    setServoAngle(L4, 180);
    if (!pressingCheck("right", frameDelay))
      return;
    setServoAngle(R1, 135);
    setServoAngle(L2, 135);
    if (!pressingCheck("right", frameDelay))
      return;
  }
  runStandPose(1);
}
