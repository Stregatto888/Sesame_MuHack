#include "display/face_engine.h"
#include "display/bitmaps.h"
#include "core/config.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================================
// PERIPHERAL INSTANCE
// ============================================================================

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================================================
// FACE DISPATCH TABLE
// ============================================================================

/**
 * @struct FaceEntry
 * @brief Bind a symbolic face name to its bitmap frame array.
 */
struct FaceEntry
{
  const char *name;
  const unsigned char *const *frames;
  uint8_t maxFrames;
};

/// Expand a FACE_LIST entry into a MAX_FACE_FRAMES-slot pointer array.
#define MAKE_FACE_FRAMES(name)                                          \
  static const unsigned char *const face_##name##_frames[] = {         \
      epd_bitmap_##name, epd_bitmap_##name##_1, epd_bitmap_##name##_2, \
      epd_bitmap_##name##_3, epd_bitmap_##name##_4, epd_bitmap_##name##_5};

#define X(name) MAKE_FACE_FRAMES(name)
FACE_LIST
#undef X
#undef MAKE_FACE_FRAMES

static const FaceEntry faceEntries[] = {
#define X(name) {#name, face_##name##_frames, MAX_FACE_FRAMES},
    FACE_LIST
#undef X
    {"default", face_defualt_frames, MAX_FACE_FRAMES}};

// ============================================================================
// PER-FACE FPS OVERRIDES
// ============================================================================

/**
 * @struct FaceFpsEntry
 * @brief Override default playback rate for a single face.
 */
struct FaceFpsEntry
{
  const char *name;
  uint8_t fps;
};

static const FaceFpsEntry faceFpsEntries[] = {
    {"walk", 1},
    {"rest", 1},
    {"swim", 1},
    {"dance", 1},
    {"wave", 1},
    {"point", 5},
    {"stand", 1},
    {"cute", 1},
    {"pushup", 1},
    {"freaky", 1},
    {"bow", 1},
    {"worm", 1},
    {"shake", 1},
    {"shrug", 1},
    {"dead", 2},
    {"crab", 1},
    {"idle", 1},
    {"idle_blink", 7},
    {"default", 1},
    // Conversational faces driven externally (no auto-animation cadence).
    {"happy", 1},
    {"talk_happy", 1},
    {"sad", 1},
    {"talk_sad", 1},
    {"angry", 1},
    {"talk_angry", 1},
    {"surprised", 1},
    {"talk_surprised", 1},
    {"sleepy", 1},
    {"talk_sleepy", 1},
    {"love", 1},
    {"talk_love", 1},
    {"excited", 1},
    {"talk_excited", 1},
    {"confused", 1},
    {"talk_confused", 1},
    {"thinking", 1},
    {"talk_thinking", 1},
};

// ============================================================================
// STATE VARIABLES
// ============================================================================

namespace Display
{
  String currentFaceName = "default";
  int    faceFps          = DEFAULT_FACE_FPS;
} // namespace Display

static const unsigned char *const *currentFaceFrames = nullptr;
static uint8_t  currentFaceFrameCount  = 0;
static uint8_t  currentFaceFrameIndex  = 0;
static unsigned long lastFaceFrameMs   = 0;
static FaceAnimMode currentFaceMode    = FACE_ANIM_LOOP;
static int8_t   faceFrameDirection     = 1;
static bool     faceAnimFinished       = false;
static int      currentFaceFps         = 0;

static bool         idleActive          = false;
static bool         idleBlinkActive     = false;
static unsigned long nextIdleBlinkMs    = 0;
static uint8_t      idleBlinkRepeatsLeft = 0;

static unsigned long lastInputTime      = 0;
static bool          firstInputReceived = false;
static bool          showingWifiInfo    = false;
static int           wifiScrollPos      = 0;
static unsigned long lastWifiScrollMs   = 0;
static String        wifiInfoText       = "";

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

static void updateFaceBitmap(const unsigned char *bitmap)
{
  display.clearDisplay();
  display.drawBitmap(0, 0, bitmap, 128, 64, SSD1306_WHITE);
  display.display();
}

static uint8_t countFrames(const unsigned char *const *frames, uint8_t maxFrames)
{
  if (frames == nullptr || frames[0] == nullptr)
    return 0;
  uint8_t count = 0;
  for (uint8_t i = 0; i < maxFrames; i++)
  {
    if (frames[i] == nullptr)
      break;
    count++;
  }
  return count;
}

static int getFaceFpsForName(const String &faceName)
{
  for (size_t i = 0; i < (sizeof(faceFpsEntries) / sizeof(faceFpsEntries[0])); i++)
  {
    if (faceName.equalsIgnoreCase(faceFpsEntries[i].name))
      return faceFpsEntries[i].fps;
  }
  return Display::faceFps;
}

static void scheduleNextIdleBlink(unsigned long minMs, unsigned long maxMs)
{
  nextIdleBlinkMs = millis() + (unsigned long)random(minMs, maxMs);
}

// ============================================================================
// DISPLAY NAMESPACE IMPLEMENTATION
// ============================================================================

bool Display::init()
{
  lastInputTime      = millis();
  firstInputReceived = false;
  showingWifiInfo    = false;

  bool ok = display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
  if (!ok)
  {
    Serial.println(F("SSD1306 allocation failed - continuing without display."));
    return false;
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  return true;
}

void Display::setMarqueeText(const String &text)
{
  wifiInfoText  = text;
  lastInputTime = millis(); // Reset idle timer from end-of-setup baseline.
}

void Display::bootMsg(const char *msg, bool clear)
{
  if (clear)
  {
    display.clearDisplay();
    display.setCursor(0, 0);
  }
  display.println(msg);
  display.display();
  Serial.println(msg);
}

void Display::set(const String &faceName)
{
  if (faceName == Display::currentFaceName && currentFaceFrames != nullptr)
    return;

  Serial.print(F("[DEBUG] Display::set: switching face -> "));
  Serial.println(faceName);

  Display::currentFaceName = faceName;
  currentFaceFrameIndex    = 0;
  lastFaceFrameMs          = 0;
  faceFrameDirection       = 1;
  faceAnimFinished         = false;
  currentFaceFps           = getFaceFpsForName(faceName);

  // Default fallback.
  currentFaceFrames     = face_defualt_frames;
  currentFaceFrameCount = countFrames(face_defualt_frames, MAX_FACE_FRAMES);

  for (size_t i = 0; i < (sizeof(faceEntries) / sizeof(faceEntries[0])); i++)
  {
    if (faceName.equalsIgnoreCase(faceEntries[i].name))
    {
      currentFaceFrames     = faceEntries[i].frames;
      currentFaceFrameCount = countFrames(faceEntries[i].frames, faceEntries[i].maxFrames);
      break;
    }
  }

  if (currentFaceFrameCount == 0)
  {
    currentFaceFrames     = face_defualt_frames;
    currentFaceFrameCount = countFrames(face_defualt_frames, MAX_FACE_FRAMES);
    Display::currentFaceName = "default";
    currentFaceFps           = getFaceFpsForName("default");
  }

  if (currentFaceFrameCount > 0 && currentFaceFrames[0] != nullptr)
    updateFaceBitmap(currentFaceFrames[0]);
}

void Display::setMode(FaceAnimMode mode)
{
  currentFaceMode    = mode;
  faceFrameDirection = 1;
  faceAnimFinished   = false;
}

void Display::setWithMode(const String &faceName, FaceAnimMode mode)
{
  Display::setMode(mode);
  Display::set(faceName);
}

void Display::tickFace()
{
  if (currentFaceFrames == nullptr || currentFaceFrameCount <= 1)
    return;
  if (currentFaceMode == FACE_ANIM_ONCE && faceAnimFinished)
    return;

  unsigned long now      = millis();
  int fps                = max(1, (currentFaceFps > 0 ? currentFaceFps : faceFps));
  unsigned long interval = 1000UL / fps;

  if (now - lastFaceFrameMs >= interval)
  {
    lastFaceFrameMs = now;

    if (currentFaceMode == FACE_ANIM_LOOP)
    {
      currentFaceFrameIndex = (currentFaceFrameIndex + 1) % currentFaceFrameCount;
    }
    else if (currentFaceMode == FACE_ANIM_ONCE)
    {
      if (currentFaceFrameIndex + 1 >= currentFaceFrameCount)
      {
        currentFaceFrameIndex = currentFaceFrameCount - 1;
        faceAnimFinished      = true;
      }
      else
      {
        currentFaceFrameIndex++;
      }
    }
    else // FACE_ANIM_BOOMERANG
    {
      if (faceFrameDirection > 0)
      {
        if (currentFaceFrameIndex + 1 >= currentFaceFrameCount)
        {
          faceFrameDirection = -1;
          if (currentFaceFrameIndex > 0)
            currentFaceFrameIndex--;
        }
        else
        {
          currentFaceFrameIndex++;
        }
      }
      else
      {
        if (currentFaceFrameIndex == 0)
        {
          faceFrameDirection = 1;
          if (currentFaceFrameCount > 1)
            currentFaceFrameIndex++;
        }
        else
        {
          currentFaceFrameIndex--;
        }
      }
    }
    updateFaceBitmap(currentFaceFrames[currentFaceFrameIndex]);
  }
}

void Display::enterIdle()
{
  idleActive           = true;
  idleBlinkActive      = false;
  idleBlinkRepeatsLeft = 0;
  Display::setWithMode("idle", FACE_ANIM_BOOMERANG);
  scheduleNextIdleBlink(3000, 7000);
}

void Display::exitIdle()
{
  idleActive      = false;
  idleBlinkActive = false;
}

void Display::tickIdle()
{
  if (!idleActive)
    return;

  if (!idleBlinkActive)
  {
    if (millis() >= nextIdleBlinkMs)
    {
      idleBlinkActive = true;
      if (idleBlinkRepeatsLeft == 0 && random(0, 100) < 30)
        idleBlinkRepeatsLeft = 1; // trigger a double-blink
      Display::setWithMode("idle_blink", FACE_ANIM_ONCE);
    }
    return;
  }

  if (currentFaceMode == FACE_ANIM_ONCE && faceAnimFinished)
  {
    idleBlinkActive = false;
    Display::setWithMode("idle", FACE_ANIM_BOOMERANG);
    if (idleBlinkRepeatsLeft > 0)
    {
      idleBlinkRepeatsLeft--;
      scheduleNextIdleBlink(120, 220);
    }
    else
    {
      scheduleNextIdleBlink(3000, 7000);
    }
  }
}

void Display::notifyInput()
{
  lastInputTime = millis();
  if (!firstInputReceived)
  {
    firstInputReceived = true;
    showingWifiInfo    = false;
  }
}

void Display::tickMarquee()
{
  if (firstInputReceived)
  {
    if (showingWifiInfo)
    {
      showingWifiInfo = false;
      if (currentFaceFrames != nullptr && currentFaceFrameCount > 0)
        updateFaceBitmap(currentFaceFrames[currentFaceFrameIndex]);
    }
    return;
  }

  unsigned long now = millis();

  if (!showingWifiInfo && (now - lastInputTime >= WIFI_MARQUEE_IDLE_MS))
  {
    showingWifiInfo = true;
    wifiScrollPos   = 0;
    lastWifiScrollMs = now;
  }

  if (!showingWifiInfo)
    return;

  if (now - lastWifiScrollMs >= WIFI_MARQUEE_TICK_MS)
  {
    lastWifiScrollMs = now;

    display.clearDisplay();
    if (currentFaceFrames != nullptr && currentFaceFrameCount > 0)
      display.drawBitmap(0, 0, currentFaceFrames[currentFaceFrameIndex], 128, 64, SSD1306_WHITE);

    // Black bar for text legibility.
    display.fillRect(0, 0, 128, 10, SSD1306_BLACK);

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setTextWrap(false);
    display.setCursor(-wifiScrollPos, 1);
    display.print(wifiInfoText);
    display.setTextWrap(true);
    display.display();

    wifiScrollPos += WIFI_MARQUEE_SCROLL_PX;
    if (wifiScrollPos >= (int)(wifiInfoText.length() * 6))
      wifiScrollPos = 0;
  }
}
