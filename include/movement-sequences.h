/**
 * @file movement-sequences.h
 * @brief Servo choreography and pose primitives for the Sesame MuHack robot
 * @author Luca - MuHack
 *
 * Defines the eight-servo kinematic addressing scheme, the OLED face animation
 * mode flags shared with the rendering pipeline, and the inline pose / gait
 * routines invoked by the main control loop.
 *
 * ## Servo Layout
 *
 * The robot exposes eight independent servos arranged as four limbs (one per
 * quadrant). Each limb has a *hip* (Rx/Lx with x in {1, 2}) and a *foot*
 * (Rx/Lx with x in {3, 4}) joint, mirrored on the right (R) and left (L)
 * sides:
 *
 * @code
 *      R1 ── HIP-FRONT-RIGHT       L1 ── HIP-FRONT-LEFT
 *      R2 ── HIP-BACK-RIGHT        L2 ── HIP-BACK-LEFT
 *      R3 ── FOOT-FRONT-RIGHT      L3 ── FOOT-FRONT-LEFT
 *      R4 ── FOOT-BACK-RIGHT       L4 ── FOOT-BACK-LEFT
 * @endcode
 *
 * The numeric indices follow the physical wiring on the carrier board and
 * therefore *do not* match the lexicographic order of the servo names.
 *
 * ## Pose Conventions
 *  - Every pose ends by re-entering the canonical @ref runStandPose and
 *    clearing `currentCommand` so the dispatcher in main.cpp returns to the
 *    idle state.
 *  - Movement gaits (`runWalkPose`, `runWalkBackward`, `runTurnLeft`,
 *    `runTurnRight`) periodically poll @ref pressingCheck so they can be
 *    interrupted within one frame when the user issues a new command.
 *  - Face animations are triggered through @ref setFaceWithMode, keeping
 *    the OLED expression in sync with the currently performed pose.
 *
 * @see face-bitmaps.h
 * @see main.cpp
 */

#pragma once

#include <Arduino.h>

// ============================================================================
// SERVO ADDRESSING
// ============================================================================

/**
 * @enum ServoName
 * @brief Symbolic identifiers for the eight robot servos.
 *
 * The numeric values map directly to the index used on the `servos[]` array
 * declared in main.cpp. Reordering these constants will *break* the runtime
 * pin assignment.
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

/// Human-readable servo labels, ordered to match @ref ServoName values.
const String ServoNames[] = {"R1", "R2", "L1", "L2", "R4", "R3", "L3", "L4"};

/**
 * @brief Resolve a servo label (e.g. `"R1"`, `"L3"`) to its @ref ServoName index.
 * @param servo Two-character canonical servo identifier.
 * @return The matching @ref ServoName value, or `-1` when the label is unknown.
 */
inline int servoNameToIndex(const String &servo)
{
  if (servo == "L1")
    return L1;
  if (servo == "L2")
    return L2;
  if (servo == "L3")
    return L3;
  if (servo == "L4")
    return L4;
  if (servo == "R1")
    return R1;
  if (servo == "R2")
    return R2;
  if (servo == "R3")
    return R3;
  if (servo == "R4")
    return R4;
  return -1;
}

// ============================================================================
// FACE ANIMATION CONTRACT
// ============================================================================

/**
 * @enum FaceAnimMode
 * @brief Playback strategy for OLED face animation sequences.
 *
 * Consumed by @ref setFaceMode / @ref setFaceWithMode in main.cpp to drive
 * the per-frame state machine implemented in `updateAnimatedFace()`.
 */
enum FaceAnimMode : uint8_t
{
  FACE_ANIM_LOOP = 0,     ///< Cycle frames forever (e.g. ambient idle).
  FACE_ANIM_ONCE = 1,     ///< Play once and freeze on the last frame.
  FACE_ANIM_BOOMERANG = 2 ///< Play forwards then backwards, repeat.
};

// ============================================================================
// EXTERNAL DEPENDENCIES PROVIDED BY main.cpp
// ============================================================================

extern int frameDelay;        ///< Per-frame pause used by gait routines (ms).
extern int walkCycles;        ///< Number of repetitions per locomotion pose.
extern String currentCommand; ///< Active command string driving the dispatcher.

/// Drive the requested servo to @p angle, applying per-channel sub-trim.
extern void setServoAngle(uint8_t channel, int angle);
/// Switch to the requested face by symbolic name.
extern void setFace(const String &faceName);
/// Override the playback mode of the currently displayed face.
extern void setFaceMode(FaceAnimMode mode);
/// Convenience helper combining @ref setFace and @ref setFaceMode.
extern void setFaceWithMode(const String &faceName, FaceAnimMode mode);
/// Cooperative delay that keeps the OLED animation running.
extern void delayWithFace(unsigned long ms);
/// Enter the idle state (ambient face + scheduled blinks).
extern void enterIdle();
/// Verify that @p cmd is still pressed during a @p ms long window.
extern bool pressingCheck(String cmd, int ms);

// ============================================================================
// POSE / GAIT PROTOTYPES
// ============================================================================

void runRestPose();
void runStandPose(int face = 1);
void runWavePose();
void runDancePose();
void runSwimPose();
void runPointPose();
void runPushupPose();
void runBowPose();
void runCutePose();
void runFreakyPose();
void runWormPose();
void runShakePose();
void runShrugPose();
void runDeadPose();
void runCrabPose();
void runWalkPose();
void runWalkBackward();
void runTurnLeft();
void runTurnRight();

// ============================================================================
// STATIC POSES
// ============================================================================

/**
 * @brief Drop every servo to its mechanical center (90 degrees).
 *
 * Used as a soft "power down" pose: the robot relaxes its limbs and the
 * accompanying boomerang face hints at sleep.
 */
inline void runRestPose()
{
  Serial.println(F("REST"));
  setFaceWithMode("rest", FACE_ANIM_BOOMERANG);
  for (int i = 0; i < 8; i++)
    setServoAngle(i, 90);
}

/**
 * @brief Canonical "standing" calibration pose used as a return point.
 *
 * Applies a fixed limb configuration that lifts the body off the ground
 * and squares the feet. Almost every other pose ends by calling this
 * routine to leave the robot in a known geometry.
 *
 * @param face When `1` (default) play the matching face animation and
 *             trigger @ref enterIdle on completion. Set to `0` from
 *             intermediate transitions to avoid flooding the OLED with
 *             redundant state changes.
 */
inline void runStandPose(int face)
{
  Serial.println(F("STAND"));
  Serial.print(F("[DEBUG] runStandPose: entering stand pose, face="));
  Serial.println(face);
  if (face == 1)
    setFaceWithMode("stand", FACE_ANIM_ONCE);
  setServoAngle(R1, 135);
  setServoAngle(R2, 45);
  setServoAngle(L1, 45);
  setServoAngle(L2, 135);
  setServoAngle(R4, 0);
  setServoAngle(R3, 180);
  setServoAngle(L3, 0);
  setServoAngle(L4, 180);
  if (face == 1)
    enterIdle();
}

/**
 * @brief Greeting wave performed with the front-left foot servo.
 *
 * Lifts the front-left foot four times while keeping the rest of the
 * skeleton anchored in the standing pose.
 */
inline void runWavePose()
{
  Serial.println(F("WAVE"));
  setFaceWithMode("wave", FACE_ANIM_ONCE);
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

/**
 * @brief Side-to-side dance routine looping the dance face animation.
 *
 * Alternates the front-leg foot positions five times to produce the
 * trademark MuHack "shimmy".
 */
inline void runDancePose()
{
  Serial.println(F("DANCE"));
  setFaceWithMode("dance", FACE_ANIM_LOOP);
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

/**
 * @brief Mimic a freestyle swim with synchronized hip motion.
 *
 * Cycles the hip joints between the standing geometry and the wide-open
 * swim geometry four times.
 */
inline void runSwimPose()
{
  Serial.println(F("SWIM"));
  setFaceWithMode("swim", FACE_ANIM_ONCE);
  for (int i = 0; i < 8; i++)
    setServoAngle(i, 90);
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

/**
 * @brief Static "pointing" gesture held for two seconds.
 *
 * Useful as a directional cue or in conjunction with the talking face
 * animations.
 */
inline void runPointPose()
{
  Serial.println(F("POINT"));
  setFaceWithMode("point", FACE_ANIM_BOOMERANG);
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

/**
 * @brief Four-rep push-up routine.
 *
 * Dips the body towards the floor and back up by alternating the front
 * foot servos.
 */
inline void runPushupPose()
{
  Serial.println(F("PUSHUP"));
  setFaceWithMode("pushup", FACE_ANIM_ONCE);
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

/**
 * @brief Slow ceremonial bow held for three seconds.
 */
inline void runBowPose()
{
  Serial.println(F("BOW"));
  setFaceWithMode("bow", FACE_ANIM_ONCE);
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

/**
 * @brief "Cute" begging animation alternating the back-foot servos.
 */
inline void runCutePose()
{
  Serial.println(F("CUTE"));
  setFaceWithMode("cute", FACE_ANIM_ONCE);
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

/**
 * @brief Erratic twitching animation paired with the freaky face.
 */
inline void runFreakyPose()
{
  Serial.println(F("FREAKY"));
  setFaceWithMode("freaky", FACE_ANIM_ONCE);
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

/**
 * @brief Body undulation reminiscent of an inchworm crawl (in place).
 */
inline void runWormPose()
{
  Serial.println(F("WORM"));
  setFaceWithMode("worm", FACE_ANIM_ONCE);
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

/**
 * @brief Lateral shake driven by the back-foot servos.
 */
inline void runShakePose()
{
  Serial.println(F("SHAKE"));
  setFaceWithMode("shake", FACE_ANIM_ONCE);
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

/**
 * @brief "Shrug" gesture combining a brief dead face with raised feet.
 */
inline void runShrugPose()
{
  Serial.println(F("SHRUG"));
  runStandPose(0);
  setFaceWithMode("dead", FACE_ANIM_ONCE);
  delayWithFace(200);
  setServoAngle(R3, 90);
  setServoAngle(R4, 90);
  setServoAngle(L3, 90);
  setServoAngle(L4, 90);
  delayWithFace(1000);
  setFaceWithMode("shrug", FACE_ANIM_ONCE);
  setServoAngle(R3, 0);
  setServoAngle(R4, 180);
  setServoAngle(L3, 180);
  setServoAngle(L4, 0);
  delayWithFace(1500);
  runStandPose(1);
  if (currentCommand == "shrug")
    currentCommand = "";
}

/**
 * @brief "Dead" pose: limbs straight and dead-face boomerang animation.
 *
 * Unlike most poses this routine does *not* return to the standing pose,
 * leaving the robot in a relaxed posture.
 */
inline void runDeadPose()
{
  Serial.println(F("DEAD"));
  runStandPose(0);
  setFaceWithMode("dead", FACE_ANIM_BOOMERANG);
  delayWithFace(200);
  setServoAngle(R3, 90);
  setServoAngle(R4, 90);
  setServoAngle(L3, 90);
  setServoAngle(L4, 90);
  if (currentCommand == "dead")
    currentCommand = "";
}

/**
 * @brief Crab-walk style sideways shuffle, five oscillations.
 */
inline void runCrabPose()
{
  Serial.println(F("CRAB"));
  setFaceWithMode("crab", FACE_ANIM_ONCE);
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
//
// All gaits share a common skeleton:
//   * Trigger the matching face animation.
//   * Step through `walkCycles` repetitions.
//   * Between each phase call @ref pressingCheck so a different command
//     can interrupt the gait within a single frame.
//   * On completion settle back into @ref runStandPose.
//

/**
 * @brief Forward walking gait built around a six-phase tripod-like pattern.
 */
inline void runWalkPose()
{
  Serial.println(F("WALK FWD"));
  setFaceWithMode("walk", FACE_ANIM_ONCE);
  // Initial Step
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

/**
 * @brief Backward walking gait, mirrored from @ref runWalkPose.
 */
inline void runWalkBackward()
{
  Serial.println(F("WALK BACK"));
  setFaceWithMode("walk", FACE_ANIM_ONCE);
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

/**
 * @brief Pivot turn to the left using two alternating leg sets.
 */
inline void runTurnLeft()
{
  Serial.println(F("TURN LEFT"));
  setFaceWithMode("walk", FACE_ANIM_ONCE);
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

/**
 * @brief Pivot turn to the right (mirror of @ref runTurnLeft).
 */
inline void runTurnRight()
{
  Serial.println(F("TURN RIGHT"));
  setFaceWithMode("walk", FACE_ANIM_ONCE);
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
