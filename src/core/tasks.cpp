// ============================================================================
// core/tasks.cpp — Three FreeRTOS tasks + cooperative timing helpers
//
//  TaskWeb     — drains the DNS/HTTP queue (Web::pump)
//  TaskDisplay — ticks the OLED face animator and marquee
//  TaskMotor   — pops commands from CmdQueue and dispatches pose/gait;
//                also owns the serial diagnostic CLI
//
// delayWithFace() and pressingCheck() are defined here because they rely on
// vTaskDelay (yielding to TaskDisplay/TaskWeb) and on CmdQueue::pop().
// ============================================================================

#include "core/tasks.h"
#include "core/command_queue.h"
#include "web/web_server.h"
#include "display/face_engine.h"
#include "motors/servo_driver.h"
#include "motors/poses.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ---------------------------------------------------------------------------
// Shared state — still defined as globals in main.cpp.
// The integer timing params were made std::atomic<int> in Phase 6;
// they are not used directly in tasks.cpp (poses.cpp/servo_driver.cpp
// hold their own extern declarations).
// ---------------------------------------------------------------------------
extern String currentCommand;

// ---------------------------------------------------------------------------
// Task stack / priority configuration
// ---------------------------------------------------------------------------
#define STACK_WEB     (8192)   ///< Bytes; HTTP/DNS callbacks can be stack-heavy
#define STACK_DISPLAY (4096)   ///< Bytes; simple tick functions
#define STACK_MOTOR   (8192)   ///< Bytes; dispatcher + serial CLI + pose routines

#define PRI_WEB     (1)
#define PRI_DISPLAY (1)
#define PRI_MOTOR   (2)

// ============================================================================
// COOPERATIVE TIMING HELPERS
// ============================================================================

void delayWithFace(unsigned long ms)
{
  // With separate tasks there is no need to manually pump web or display —
  // TaskWeb and TaskDisplay run independently. Just yield for the requested
  // duration so the scheduler can serve them.
  vTaskDelay(pdMS_TO_TICKS(ms));
}

bool pressingCheck(String cmd, int ms)
{
  unsigned long start = millis();
  while (millis() - start < (unsigned long)ms)
  {
    String newCmd;
    if (CmdQueue::pop(newCmd))
    {
      // A new command arrived — store it so the TaskMotor dispatcher sees it,
      // stand the robot up, and signal the caller to abort.
      currentCommand = newCmd;
      runStandPose(1);
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  return true;
}

// ============================================================================
// TASK BODIES
// ============================================================================

// ---------------------------------------------------------------------------
// TaskWeb — drain DNS captive-portal queue and service one HTTP request
// ---------------------------------------------------------------------------
static void taskWeb(void *pvParams)
{
  for (;;)
  {
    Web::pump();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// ---------------------------------------------------------------------------
// TaskDisplay — advance OLED face frames, idle blink, and marquee scroll
// ---------------------------------------------------------------------------
static void taskDisplay(void *pvParams)
{
  for (;;)
  {
    Display::tickFace();
    Display::tickIdle();
    Display::tickMarquee();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ---------------------------------------------------------------------------
// TaskMotor — command dispatcher + serial diagnostic CLI
// ---------------------------------------------------------------------------

/// Record a user input: dismiss marquee and reset idle timer.
static inline void recordInput()
{
  Display::notifyInput();
}

static void taskMotor(void *pvParams)
{
  static char  command_buffer[32];
  static byte  buffer_pos = 0;

  for (;;)
  {
    // --- Pop next queued command ---
    {
      String incoming;
      if (CmdQueue::pop(incoming))
        currentCommand = incoming;
    }

    // --- Dispatch active command ---
    if (currentCommand != "")
    {
      String cmd = currentCommand;
      Serial.print(F("[DEBUG] TaskMotor: dispatching command -> "));
      Serial.println(cmd);
      if (cmd == "forward")
        runWalkPose();
      else if (cmd == "backward")
        runWalkBackward();
      else if (cmd == "left")
        runTurnLeft();
      else if (cmd == "right")
        runTurnRight();
      else if (cmd == "rest")
      {
        runRestPose();
        if (currentCommand == "rest")
          currentCommand = "";
      }
      else if (cmd == "stand")
      {
        runStandPose(1);
        if (currentCommand == "stand")
          currentCommand = "";
      }
      else if (cmd == "wave")
        runWavePose();
      else if (cmd == "dance")
        runDancePose();
      else if (cmd == "swim")
        runSwimPose();
      else if (cmd == "point")
        runPointPose();
      else if (cmd == "pushup")
        runPushupPose();
      else if (cmd == "bow")
        runBowPose();
      else if (cmd == "cute")
        runCutePose();
      else if (cmd == "freaky")
        runFreakyPose();
      else if (cmd == "worm")
        runWormPose();
      else if (cmd == "shake")
        runShakePose();
      else if (cmd == "shrug")
        runShrugPose();
      else if (cmd == "dead")
        runDeadPose();
      else if (cmd == "crab")
        runCrabPose();
    }

    // --- Serial CLI ---
    if (Serial.available())
    {
      char c = Serial.read();
      if (c == '\n' || c == '\r')
      {
        if (buffer_pos > 0)
        {
          command_buffer[buffer_pos] = '\0';
          int motorNum, angle;
          recordInput();
          if (strcmp(command_buffer, "run walk") == 0 || strcmp(command_buffer, "rn wf") == 0)
          {
            currentCommand = "forward";
            runWalkPose();
            currentCommand = "";
          }
          else if (strcmp(command_buffer, "rn wb") == 0)
          {
            currentCommand = "backward";
            runWalkBackward();
            currentCommand = "";
          }
          else if (strcmp(command_buffer, "rn tl") == 0)
          {
            currentCommand = "left";
            runTurnLeft();
            currentCommand = "";
          }
          else if (strcmp(command_buffer, "rn tr") == 0)
          {
            currentCommand = "right";
            runTurnRight();
            currentCommand = "";
          }
          else if (strcmp(command_buffer, "run rest") == 0 || strcmp(command_buffer, "rn rs") == 0)
            runRestPose();
          else if (strcmp(command_buffer, "run stand") == 0 || strcmp(command_buffer, "rn st") == 0)
            runStandPose(1);
          else if (strcmp(command_buffer, "rn wv") == 0)
          {
            currentCommand = "wave";
            runWavePose();
          }
          else if (strcmp(command_buffer, "rn dn") == 0)
          {
            currentCommand = "dance";
            runDancePose();
          }
          else if (strcmp(command_buffer, "rn sw") == 0)
          {
            currentCommand = "swim";
            runSwimPose();
          }
          else if (strcmp(command_buffer, "rn pt") == 0)
          {
            currentCommand = "point";
            runPointPose();
          }
          else if (strcmp(command_buffer, "rn pu") == 0)
          {
            currentCommand = "pushup";
            runPushupPose();
          }
          else if (strcmp(command_buffer, "rn bw") == 0)
          {
            currentCommand = "bow";
            runBowPose();
          }
          else if (strcmp(command_buffer, "rn ct") == 0)
          {
            currentCommand = "cute";
            runCutePose();
          }
          else if (strcmp(command_buffer, "rn fk") == 0)
          {
            currentCommand = "freaky";
            runFreakyPose();
          }
          else if (strcmp(command_buffer, "rn wm") == 0)
          {
            currentCommand = "worm";
            runWormPose();
          }
          else if (strcmp(command_buffer, "rn sk") == 0)
          {
            currentCommand = "shake";
            runShakePose();
          }
          else if (strcmp(command_buffer, "rn sg") == 0)
          {
            currentCommand = "shrug";
            runShrugPose();
          }
          else if (strcmp(command_buffer, "rn dd") == 0)
          {
            currentCommand = "dead";
            runDeadPose();
          }
          else if (strcmp(command_buffer, "rn cb") == 0)
          {
            currentCommand = "crab";
            runCrabPose();
          }
          else if (strcmp(command_buffer, "subtrim") == 0 || strcmp(command_buffer, "st") == 0)
          {
            Serial.println("Subtrim values:");
            for (int i = 0; i < SERVO_COUNT; i++)
            {
              Serial.print("Motor ");
              Serial.print(i);
              Serial.print(": ");
              if (Motors::subtrim[i] >= 0)
                Serial.print("+");
              Serial.println(Motors::subtrim[i]);
            }
          }
          else if (strcmp(command_buffer, "subtrim save") == 0 || strcmp(command_buffer, "st save") == 0)
          {
            Serial.println("Copy and paste this into your code:");
            Serial.print("int8_t Motors::subtrim[SERVO_COUNT] = {");
            for (int i = 0; i < SERVO_COUNT; i++)
            {
              Serial.print(Motors::subtrim[i]);
              if (i < SERVO_COUNT - 1)
                Serial.print(", ");
            }
            Serial.println("};");
          }
          else if (strncmp(command_buffer, "subtrim reset", 13) == 0 || strncmp(command_buffer, "st reset", 8) == 0)
          {
            for (int i = 0; i < SERVO_COUNT; i++)
              Motors::subtrim[i] = 0;
            Serial.println("All subtrim values reset to 0");
          }
          else if (strncmp(command_buffer, "subtrim ", 8) == 0 || strncmp(command_buffer, "st ", 3) == 0)
          {
            const char *params = (command_buffer[1] == 't') ? command_buffer + 3 : command_buffer + 8;
            int trimMotor, trimValue;
            if (sscanf(params, "%d %d", &trimMotor, &trimValue) == 2)
            {
              if (trimMotor >= 0 && trimMotor < SERVO_COUNT)
              {
                if (trimValue >= -90 && trimValue <= 90)
                {
                  Motors::subtrim[trimMotor] = trimValue;
                  Serial.print("Motor ");
                  Serial.print(trimMotor);
                  Serial.print(" subtrim set to ");
                  if (trimValue >= 0)
                    Serial.print("+");
                  Serial.println(trimValue);
                }
                else
                {
                  Serial.println("Subtrim value must be between -90 and +90");
                }
              }
              else
              {
                Serial.println("Invalid motor number (0-7)");
              }
            }
          }
          else if (strncmp(command_buffer, "all ", 4) == 0)
          {
            if (sscanf(command_buffer + 4, "%d", &angle) == 1)
            {
              for (int i = 0; i < SERVO_COUNT; i++)
                Motors::setAngle(i, angle);
              Serial.print("All servos set to ");
              Serial.println(angle);
            }
          }
          else if (sscanf(command_buffer, "%d %d", &motorNum, &angle) == 2)
          {
            if (motorNum >= 0 && motorNum < SERVO_COUNT)
            {
              Motors::setAngle(motorNum, angle);
              Serial.print("Servo ");
              Serial.print(motorNum);
              Serial.print(" set to ");
              Serial.println(angle);
            }
            else
            {
              Serial.println("Invalid motor number (0-7)");
            }
          }
          buffer_pos = 0;
        }
      }
      else if (buffer_pos < sizeof(command_buffer) - 1)
      {
        command_buffer[buffer_pos++] = c;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// ============================================================================
// PUBLIC API
// ============================================================================

void Tasks::startAll()
{
  xTaskCreate(taskWeb,     "TaskWeb",     STACK_WEB,     nullptr, PRI_WEB,     nullptr);
  xTaskCreate(taskDisplay, "TaskDisplay", STACK_DISPLAY, nullptr, PRI_DISPLAY, nullptr);
  xTaskCreate(taskMotor,   "TaskMotor",   STACK_MOTOR,   nullptr, PRI_MOTOR,   nullptr);
}
