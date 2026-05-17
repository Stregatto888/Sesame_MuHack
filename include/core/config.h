#pragma once

#include <Arduino.h>
#include <IPAddress.h>

// ============================================================================
// NETWORK CONFIGURATION
// ============================================================================

/// SSID broadcast by the on-board soft access point.
#define AP_SSID "Sesame MuHack"
/// Pre-shared key for AP_SSID. Must be at least 8 characters.
#define AP_PASS "12345678"

/// Optional upstream SSID joined when ENABLE_NETWORK_MODE is true.
#define NETWORK_SSID ""
/// Pre-shared key for NETWORK_SSID.
#define NETWORK_PASS ""
/// When `true` the robot also tries to associate to NETWORK_SSID at boot.
#define ENABLE_NETWORK_MODE false

/// Static IP exposed by the soft AP. Avoids `softAPIP()` hangs on ESP32-S2.
#define AP_IP      IPAddress(192, 168, 4, 1)
#define AP_GATEWAY IPAddress(192, 168, 4, 1)
#define AP_SUBNET  IPAddress(255, 255, 255, 0)

/// mDNS / captive-portal hostname (resolves as <hostname>.local on the LAN).
#define DEVICE_HOSTNAME "sesame-robot"

// ============================================================================
// HARDWARE CONFIGURATION — OLED DISPLAY
// ============================================================================

#define SCREEN_WIDTH  128  ///< OLED width in pixels.
#define SCREEN_HEIGHT 64   ///< OLED height in pixels.
#define OLED_RESET    -1   ///< Reset pin shared with the MCU reset line.
#define OLED_I2C_ADDR 0x3C ///< 7-bit I²C address of the SSD1306.

/// I²C data line (Lolin S2 Mini wiring).
#define I2C_SDA 35
/// I²C clock line (Lolin S2 Mini wiring).
#define I2C_SCL 33

// ============================================================================
// HARDWARE CONFIGURATION — SERVOS
// ============================================================================

/// Number of servo channels.
#define SERVO_COUNT 8

/// Active servo-to-GPIO mapping for the Lolin S2 Mini carrier.
/// Index order matches ServoName enum: R1=0, R2=1, L1=2, L2=3, R4=4, R3=5, L3=6, L4=7
static const int SERVO_PINS[SERVO_COUNT] = {1, 2, 4, 6, 8, 10, 13, 14};

/// Servo PWM frequency (Hz).
#define SERVO_PWM_HZ 50
/// Minimum pulse width in microseconds (corresponds to 0°).
#define SERVO_PULSE_MIN_US 732
/// Maximum pulse width in microseconds (corresponds to 180°).
#define SERVO_PULSE_MAX_US 2929
/// Initial angle written to every servo at boot (degrees).
#define SERVO_INIT_ANGLE 90
/// Stagger delay between attaching successive servos to spread current draw (ms).
#define SERVO_ATTACH_STAGGER_MS 150

// ============================================================================
// ANIMATION / MOTION DEFAULTS
// ============================================================================

/// Default inter-frame delay used by gait routines (ms). Runtime-tunable.
#define DEFAULT_FRAME_DELAY 100
/// Default gait repetitions per walk/turn invocation. Runtime-tunable.
#define DEFAULT_WALK_CYCLES 10
/// Default pause after each servo write to spread current draw (ms). Runtime-tunable.
#define DEFAULT_MOTOR_CURRENT_DELAY 20
/// Default face animation frame rate (FPS). Runtime-tunable.
#define DEFAULT_FACE_FPS 8

// ============================================================================
// FACE ENGINE
// ============================================================================

/// Maximum number of animation frames per face supported by MAKE_FACE_FRAMES.
#define MAX_FACE_FRAMES 6

// ============================================================================
// MISC
// ============================================================================

/// DNS port used by the captive-portal DNS server.
#define DNS_PORT 53

/// Seconds of inactivity before the Wi-Fi info marquee starts scrolling.
#define WIFI_MARQUEE_IDLE_MS 30000UL
/// Marquee horizontal scroll step in pixels per tick.
#define WIFI_MARQUEE_SCROLL_PX 2
/// Interval between marquee scroll ticks (ms).
#define WIFI_MARQUEE_TICK_MS 150
