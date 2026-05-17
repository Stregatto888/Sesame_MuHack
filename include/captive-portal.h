/**
 * @file captive-portal.h
 * @brief Embedded HTML/CSS/JS for the Sesame MuHack captive portal
 * @author Luca - MuHack
 *
 * Self-contained, single-page web interface served by the on-board
 * `WebServer` instance whenever a client connects to the
 * `Sesame MuHack` Wi-Fi access point. The page is stored in PROGMEM and
 * delivered verbatim by `handleRoot()` in main.cpp.
 *
 * ## Responsibilities
 *  - Captive portal landing page (auto-opened by mobile devices via DNS
 *    redirect to the AP IP).
 *  - Robot remote control panel (movement, poses, face presets).
 *  - Live status read-out (current command, face, hack-lock state).
 *  - Interactive `MuHack` themed terminal hitting `/terminal`.
 *
 * The page uses no external runtime dependencies aside from a Google
 * Fonts stylesheet; the rest of the assets (CSS, JavaScript) are inlined
 * to guarantee the portal works without internet access.
 *
 * @see main.cpp
 */

#pragma once

#include <Arduino.h>

// ============================================================================
// EMBEDDED WEB INTERFACE
// ============================================================================
//
// MuHack-themed Sesame Robot controller landing page. The whole asset is
// stored as a PROGMEM raw string literal so the ESP32-S2 doesn't need to
// load a filesystem to serve it.
//
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>MuHack Sesame Controller</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    @import url('https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;500;700&family=Bebas+Neue&family=Space+Grotesk:wght@400;500;600;700&display=swap');
    
    :root {
      --red: #E63946;
      --red-dark: #C62833;
      --cyan: #00D4FF;
      --void: #1A1A2E;
      --void-mid: #1F1F38;
      --void-light: #252542;
      --term-bg: #0d1117;
      --term-green: #3fb950;
      --white: #FFF;
      --chrome: #F0F2F5;
      --chrome-dark: #D1D5DB;
      --steel: #6B7280;
      --accent: var(--red);
      --accent-alt: var(--cyan);
      --content-color: var(--cyan);
      --content-color-dark: #00b8d9;
      --content-color-darker: #009bb5;
      --content-color-glow: rgba(0, 212, 255, 0.3);
    }

    *, *::before, *::after { margin: 0; padding: 0; box-sizing: border-box; }
    * { user-select: none; -webkit-user-select: none; -webkit-touch-callout: none; }

    body {
      font-family: 'Space Grotesk', 'Segoe UI', sans-serif;
      text-align: center;
      background: var(--void);
      color: var(--chrome);
      touch-action: manipulation;
      margin: 0; padding: 0;
      overflow-x: hidden;
      min-height: 100vh;
    }

    /* ======== HEADER ======== */
    .site-header {
      background: var(--void);
      clip-path: polygon(0 0, 100% 0, 100% calc(100% - 8px), 0 100%);
      padding: 0.6rem 1.5rem 1rem;
      display: flex; justify-content: space-between; align-items: center;
      position: relative; z-index: 10;
    }
    .logo-area { display: flex; align-items: center; gap: 0.6rem; text-decoration: none; }
    .logo-dot {
      width: 36px; height: 36px;
      border: 2px dashed var(--red); border-radius: 50%;
      display: flex; align-items: center; justify-content: center;
      animation: spin 20s linear infinite; position: relative;
    }
    .logo-dot::after {
      content: 'M'; font-family: 'Bebas Neue', sans-serif; font-size: 1.2rem; color: var(--white);
      animation: spin-reverse 20s linear infinite;
    }
    @keyframes spin { to { transform: rotate(360deg); } }
    @keyframes spin-reverse { to { transform: rotate(-360deg); } }
    .site-title { font-family: 'Bebas Neue', sans-serif; font-size: 1.5rem; letter-spacing: 0.1em; color: var(--white); }
    .header-tag { font-family: 'JetBrains Mono', monospace; font-size: 0.6rem; color: var(--steel); letter-spacing: 0.1em; text-transform: uppercase; }

    /* ======== HERO BANNER ======== */
    .hero-banner {
      background: var(--red); color: var(--white);
      clip-path: polygon(0 8px, 100% 0, 100% calc(100% - 8px), 0 100%);
      margin-top: -8px; padding: 1.2rem 1.5rem;
      display: flex; align-items: center; justify-content: center; gap: 1rem;
    }
    .hero-banner h1 { font-family: 'Bebas Neue', sans-serif; font-size: 2rem; letter-spacing: 0.05em; line-height: 1; }
    .hero-banner .hero-sub { font-family: 'JetBrains Mono', monospace; font-size: 0.65rem; opacity: 0.85; letter-spacing: 0.08em; }

    /* ======== MAIN CONTENT ======== */
    .main-content { background: var(--void-light); margin-top: -8px; padding: 1.5rem 1rem 2rem; }
    .sections-container { display: flex; flex-direction: column; gap: 1rem; max-width: 1400px; margin: 0 auto; }

    /* ======== SECTION CARDS ======== */
    .section {
      background: var(--void-mid); border: 1px solid rgba(107, 114, 128, 0.3);
      padding: 1.2rem; margin: 0 auto; width: 100%; max-width: 450px;
      text-align: center; transition: all 0.3s;
    }
    .section:hover { border-color: var(--accent); }
    .section-title {
      font-family: 'Bebas Neue', sans-serif; font-size: 1.4rem; letter-spacing: 0.05em;
      color: var(--white); margin: 0 0 1rem 0; position: relative; display: inline-block;
    }
    .section-title::before { content: '//'; color: var(--accent); font-size: 0.8rem; margin-right: 0.4rem; vertical-align: middle; }
    .section-label {
      font-family: 'JetBrains Mono', monospace; font-size: 0.6rem; letter-spacing: 0.15em;
      text-transform: uppercase; color: var(--cyan); margin-bottom: 0.3rem;
      display: flex; align-items: center; justify-content: center; gap: 0.3rem;
    }
    .section-label::before { content: '>'; }

    /* ======== COMMAND QUEUE ======== */
    .command-queue { font-family: 'JetBrains Mono', monospace; font-size: 0.65rem; color: var(--steel); margin-bottom: 1rem; letter-spacing: 0.05em; }
    .command-queue.full { color: var(--red); font-weight: 700; }

    /* ======== BUTTONS ======== */
    button {
      font-family: 'Space Grotesk', sans-serif; background: var(--void);
      border: 1px solid var(--steel); color: var(--chrome-dark);
      padding: 12px; font-size: 16px; cursor: pointer; transition: all 0.2s; font-weight: 500;
    }
    button:active { transform: translateY(2px); }
    button:hover { border-color: var(--accent); color: var(--white); background: var(--accent); }

    /* ======== D-PAD ======== */
    .dpad-container { display: flex; flex-direction: column; align-items: center; gap: 12px; width: 100%; }
    .dpad { display: grid; grid-template-columns: repeat(3, 1fr); grid-template-rows: repeat(2, 1fr); gap: 10px; width: 100%; max-width: 280px; aspect-ratio: 3 / 2; }
    .dpad button { font-size: 30px; border: 1px solid var(--steel); color: var(--white); background: var(--void); width: 100%; height: 100%; min-height: 65px; }
    .dpad button:hover { background: var(--cyan); border-color: var(--cyan); color: var(--void); }
    .spacer { visibility: hidden; }

    /* ======== POSE GRID ======== */
    .grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
    .btn-pose {
      background: var(--void); border: 1px solid var(--red); color: var(--white);
      padding: 10px 6px; font-size: 13px; font-family: 'JetBrains Mono', monospace;
      letter-spacing: 0.03em; clip-path: polygon(0 0, calc(100% - 6px) 0, 100% 100%, 0 100%);
    }
    .btn-pose:hover { background: var(--red); color: var(--white); }
    .btn-pose:active { background: var(--red-dark); }

    /* ======== STOP BUTTON ======== */
    .btn-stop-all {
      background: var(--red); border: none; width: 100%;
      font-family: 'Bebas Neue', sans-serif; font-size: 1.3rem; letter-spacing: 0.15em;
      padding: 14px; color: var(--white);
      clip-path: polygon(0 0, calc(100% - 10px) 0, 100% 100%, 0 100%);
    }
    .btn-stop-all:hover { background: var(--red-dark); }
    .btn-stop-all:active { transform: translateY(3px); }

    /* ======== SETTINGS BUTTON ======== */
    .btn-settings {
      background: var(--void); border: 1px solid var(--steel); padding: 10px 20px;
      font-size: 14px; font-family: 'JetBrains Mono', monospace; letter-spacing: 0.05em; text-transform: uppercase;
    }
    .btn-settings:hover { border-color: var(--cyan); color: var(--cyan); background: transparent; }

    /* ======== MOTOR CONTROLS ======== */
    .lock-indicator { font-family: 'JetBrains Mono', monospace; font-size: 0.65rem; color: var(--red); text-align: center; margin-top: 5px; display: none; }
    .lock-indicator.active { display: block; }
    .motor-controls { margin-top: 10px; }
    .motor-slider { margin: 12px 0; }
    .motor-slider label { display: flex; justify-content: space-between; font-family: 'JetBrains Mono', monospace; font-size: 0.65rem; color: var(--steel); margin-bottom: 4px; }
    .motor-slider input[type="range"] { width: 100%; height: 4px; background: var(--void); border-radius: 0; outline: none; -webkit-appearance: none; border: 1px solid var(--steel); }
    .motor-slider input[type="range"]::-webkit-slider-thumb { -webkit-appearance: none; width: 14px; height: 14px; background: var(--cyan); border-radius: 0; cursor: pointer; border: none; }
    .motor-slider input[type="range"]::-moz-range-thumb { width: 14px; height: 14px; background: var(--cyan); border-radius: 0; cursor: pointer; border: none; }
    .motor-slider input[type="range"]:disabled { opacity: 0.4; cursor: not-allowed; }
    .motor-slider input[type="range"]:disabled::-webkit-slider-thumb, .motor-slider input[type="range"]:disabled::-moz-range-thumb { background: var(--steel); cursor: not-allowed; }

    /* ======== GAMEPAD STATUS ======== */
    .gamepad-status { font-family: 'JetBrains Mono', monospace; font-size: 0.65rem; padding: 6px 12px; border: 1px solid var(--steel); color: var(--steel); background: transparent; display: inline-block; letter-spacing: 0.05em; }
    .gamepad-status.connected { border-color: var(--term-green); color: var(--term-green); }

    /* ======== TERMINAL ======== */
    .terminal-section { padding: 0 !important; border: none !important; background: transparent !important; overflow: hidden; }
    .terminal-box { background: var(--term-bg); border: 1px solid rgba(107, 114, 128, 0.3); padding: 0; text-align: left; font-family: 'JetBrains Mono', 'Courier New', monospace; overflow: hidden; }
    .terminal-box:hover { border-color: var(--term-green); }
    .terminal-header { background: #3c3c3c; padding: 0.4rem 0.75rem; display: flex; align-items: center; justify-content: space-between; }
    .terminal-header-left { display: flex; align-items: center; gap: 0.5rem; }
    .terminal-header-btns { display: flex; gap: 5px; }
    .terminal-dot { width: 10px; height: 10px; border-radius: 50%; }
    .terminal-dot.red { background: #ff5f57; }
    .terminal-dot.yellow { background: #ffbd2e; }
    .terminal-dot.green { background: #28c840; }
    .terminal-title { font-family: 'JetBrains Mono', monospace; color: rgba(255,255,255,0.9); font-size: 0.7rem; }
    .terminal-lock-badge { font-family: 'JetBrains Mono', monospace; font-size: 0.55rem; padding: 2px 6px; background: rgba(255,255,255,0.1); border-radius: 2px; color: rgba(255,255,255,0.7); letter-spacing: 0.05em; }
    .terminal-lock-badge.locked { background: rgba(230, 57, 70, 0.3); color: var(--red); animation: lockPulse 2s infinite; }
    @keyframes lockPulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }
    .terminal-output { padding: 1rem 1.2rem; max-height: 200px; overflow-y: auto; font-size: 0.75rem; line-height: 1.9; color: var(--term-green); scrollbar-width: none; -ms-overflow-style: none; }
    .terminal-output::-webkit-scrollbar { display: none; }
    .terminal-output .cmd-line { color: var(--white); }
    .terminal-output .response-line { color: rgba(255,255,255,0.85); }
    .terminal-output .error-line { color: var(--red); }
    .terminal-output .hack-line { color: var(--red); text-shadow: 0 0 8px rgba(230, 57, 70, 0.5); font-weight: 700; }
    .terminal-output .muhack-line { color: var(--cyan); text-shadow: 0 0 8px rgba(0, 212, 255, 0.5); font-weight: 700; }
    .terminal-input-row { display: flex; align-items: center; padding: 0.6rem 1.2rem; border-top: 1px solid rgba(107, 114, 128, 0.2); }
    .terminal-prompt { color: var(--term-green); font-size: 0.75rem; white-space: nowrap; user-select: none; }
    .terminal-path { color: var(--cyan); font-size: 0.75rem; }
    .terminal-input { flex: 1; background: transparent; border: none; color: var(--white); font-family: 'JetBrains Mono', monospace; font-size: 0.75rem; outline: none; caret-color: var(--term-green); padding: 0 0 0 4px; }
    .terminal-input::placeholder { color: var(--steel); }

    /* ======== SETTINGS PANEL ======== */
    .settings-panel { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(26, 26, 46, 0.95); z-index: 100; backdrop-filter: blur(4px); overflow-y: auto; }
    .settings-content { background: var(--void-mid); border: 1px solid var(--steel); max-width: 400px; margin: 30px auto; padding: 25px; text-align: left; }
    .settings-content h3 { font-family: 'Bebas Neue', sans-serif; font-size: 1.8rem; color: var(--white); margin-top: 0; text-align: center; letter-spacing: 0.05em; }
    .settings-section { margin: 16px 0; padding: 12px; background: var(--void); border: 1px solid rgba(107, 114, 128, 0.2); }
    .settings-section h4 { font-family: 'JetBrains Mono', monospace; color: var(--cyan); margin: 0 0 8px 0; font-size: 0.65rem; text-transform: uppercase; letter-spacing: 0.15em; }
    .settings-section h4::before { content: '> '; color: var(--cyan); }
    .settings-content label { display: block; margin-top: 10px; font-family: 'JetBrains Mono', monospace; font-weight: 400; color: var(--chrome-dark); font-size: 0.7rem; }
    .settings-content input, .settings-content select { width: 100%; padding: 8px; margin-top: 4px; background: var(--void); color: var(--white); border: 1px solid var(--steel); font-family: 'JetBrains Mono', monospace; font-size: 0.75rem; box-sizing: border-box; }
    .settings-content input:focus, .settings-content select:focus { border-color: var(--cyan); outline: none; }
    .btn-save { background: var(--accent); border: none; width: 100%; margin-top: 20px; color: var(--white); font-family: 'Bebas Neue', sans-serif; letter-spacing: 0.1em; font-size: 1.1rem; padding: 12px; clip-path: polygon(0 0, calc(100% - 8px) 0, 100% 100%, 0 100%); }
    .btn-save:hover { background: var(--void); color: var(--white); }
    .btn-close { background: transparent; border: 1px solid var(--steel); width: 100%; margin-top: 10px; color: var(--chrome-dark); font-family: 'JetBrains Mono', monospace; font-size: 0.7rem; padding: 10px; letter-spacing: 0.05em; }
    .btn-close:hover { border-color: var(--red); color: var(--red); }

    /* ======== FOOTER ======== */
    .site-footer { background: var(--void); clip-path: polygon(0 12px, 100% 0, 100% 100%, 0 100%); margin-top: -1px; padding: 1.5rem 1.5rem 1rem; display: flex; justify-content: center; align-items: center; gap: 1.5rem; }
    .footer-text { font-family: 'JetBrains Mono', monospace; font-size: 0.6rem; color: var(--steel); }
    .footer-text a { color: var(--cyan); text-decoration: none; border-bottom: 1px dashed var(--cyan); }
    .footer-text a:hover { opacity: 0.7; }
    .status-dot { width: 6px; height: 6px; background: var(--accent); border-radius: 50%; display: inline-block; animation: pulse 2s ease-in-out infinite; }
    @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.4; } }

    /* ======== DESKTOP LAYOUT ======== */
    @media (min-width: 1024px) {
      .main-content { padding: 2rem; }
      .sections-container { flex-direction: row; justify-content: center; align-items: flex-start; gap: 1.5rem; padding: 0 1rem; }
      .section-column { flex: 0 1 450px; display: flex; flex-direction: column; gap: 1rem; }
      .section { width: 100%; max-width: 450px; margin: 0; }
    }
    @media (max-width: 480px) {
      .hero-banner h1 { font-size: 1.5rem; }
      .terminal-output { font-size: 0.65rem; }
      .btn-pose { font-size: 11px; padding: 8px 4px; }
    }
  </style>
</head>
<body>

  <!-- HEADER -->
  <div class="site-header">
    <div class="logo-area">
      <div class="logo-dot"></div>
      <div><div class="site-title">MUHACK</div></div>
    </div>
    <div class="header-tag">BRESCIA, ITALY &mdash; DAL 2014</div>
  </div>

  <!-- HERO -->
  <div class="hero-banner">
    <div>
      <h1>SESAME CONTROLLER</h1>
      <div class="hero-sub">Quadruped Robot // WiFi Control Interface</div>
    </div>
  </div>

  <!-- MAIN -->
  <div class="main-content">
    <div class="command-queue" id="queueStatus">Command Queue: 0/3</div>
    <div class="sections-container">
      <div class="section-column">
        <!-- Movement -->
        <div class="section">
          <div class="section-label">CONTROLS</div>
          <div class="section-title">Movement</div>
          <div class="dpad-container">
            <div class="dpad">
              <div class="spacer"></div>
              <button onmousedown="move('forward')" onmouseup="stop()" ontouchstart="move('forward')" ontouchend="stop()">&#9650;</button>
              <div class="spacer"></div>
              <button onmousedown="move('left')" onmouseup="stop()" ontouchstart="move('left')" ontouchend="stop()">&#9664;</button>
              <button onmousedown="move('backward')" onmouseup="stop()" ontouchstart="move('backward')" ontouchend="stop()">&#9660;</button>
              <button onmousedown="move('right')" onmouseup="stop()" ontouchstart="move('right')" ontouchend="stop()">&#9654;</button>
            </div>
            <button class="btn-stop-all" onclick="stop()">STOP ALL</button>
          </div>
        </div>
        <!-- Poses -->
        <div class="section">
          <div class="section-label">ANIMATIONS</div>
          <div class="section-title">Poses &amp; Emotes</div>
          <div class="grid">
            <button class="btn-pose" onclick="pose('rest')">Rest</button>
            <button class="btn-pose" onclick="pose('stand')">Stand</button>
            <button class="btn-pose" onclick="pose('wave')">Wave</button>
            <button class="btn-pose" onclick="pose('dance')">Dance</button>
            <button class="btn-pose" onclick="pose('swim')">Swim</button>
            <button class="btn-pose" onclick="pose('point')">Point</button>
            <button class="btn-pose" onclick="pose('pushup')">Pushup</button>
            <button class="btn-pose" onclick="pose('bow')">Bow</button>
            <button class="btn-pose" onclick="pose('cute')">Cute</button>
            <button class="btn-pose" onclick="pose('freaky')">Freaky</button>
            <button class="btn-pose" onclick="pose('worm')">Worm</button>
            <button class="btn-pose" onclick="pose('shake')">Shake</button>
            <button class="btn-pose" onclick="pose('shrug')">Shrug</button>
            <button class="btn-pose" onclick="pose('dead')">Dead</button>
            <button class="btn-pose" onclick="pose('crab')">Crab</button>
          </div>
        </div>
      </div>
      <div class="section-column">
        <!-- System -->
        <div class="section">
          <div class="section-label">HARDWARE</div>
          <div class="section-title">System</div>
          <button class="btn-settings" onclick="openSettings()">Settings</button>
          <div style="margin-top: 12px;">
            <div id="gamepadStatus" class="gamepad-status">Gamepad disconnected</div>
          </div>
        </div>
        <!-- Terminal -->
        <div class="section terminal-section">
          <div class="terminal-box">
            <div class="terminal-header">
              <div class="terminal-header-left">
                <div class="terminal-header-btns">
                  <span class="terminal-dot red"></span>
                  <span class="terminal-dot yellow"></span>
                  <span class="terminal-dot green"></span>
                </div>
                <span class="terminal-title">muhack@sesame:~</span>
              </div>
              <span class="terminal-lock-badge" id="termLockBadge">UNLOCKED</span>
            </div>
            <div class="terminal-output" id="termOutput"><span class="muhack-line">MuHack Sesame Terminal v1.0</span><br><span class="response-line">Type 'help' for commands.</span><br></div>
            <div class="terminal-input-row">
              <span class="terminal-prompt">&#10148;&nbsp;</span>
              <span class="terminal-path">~&nbsp;</span>
              <input class="terminal-input" id="termInput" type="text" placeholder="type a command..." autocomplete="off" autocorrect="off" autocapitalize="off" spellcheck="false">
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <!-- FOOTER -->
  <div class="site-footer">
    <span class="footer-text"><span class="status-dot"></span>&nbsp; MuHack &mdash; Brescia Hackerspace</span>
    <span class="footer-text"><a href="https://muhack.org" target="_blank">muhack.org</a></span>
  </div>

  <!-- SETTINGS PANEL -->
  <div id="settingsPanel" class="settings-panel">
    <div class="settings-content">
      <h3>Settings</h3>
      <div class="settings-section">
        <h4>Animation Parameters</h4>
        <label>Frame Delay (ms):</label>
        <input type="number" id="frameDelay" min="1" max="1000" step="1">
        <label>Walk Cycles:</label>
        <input type="number" id="walkCycles" min="1" max="50" step="1">
      </div>
      <div class="settings-section">
        <h4>Motor Settings</h4>
        <label>Motor Current Delay (ms):</label>
        <input type="number" id="motorCurrentDelay" min="0" max="500" step="1">
        <label>Motor Speed:</label>
        <select id="motorSpeed">
          <option value="slow">Slow</option>
          <option value="medium" selected>Medium</option>
          <option value="fast">Fast</option>
        </select>
      </div>
      <div class="settings-section">
        <h4>Theme Accent</h4>
        <label>Accent Color:</label>
        <select id="themeColor">
          <option value="#E63946">Red (MuHack)</option>
          <option value="#00D4FF">Cyan (MuHack Alt)</option>
          <option value="#ff8c42">Orange</option>
          <option value="#2ecc71">Green</option>
          <option value="#9b59b6">Purple</option>
          <option value="#f39c12">Yellow</option>
          <option value="#e91e63">Pink</option>
          <option value="custom">Custom</option>
        </select>
        <input type="color" id="customColor" value="#E63946" style="margin-top: 8px; display: none;">
      </div>
      <button class="btn-settings" style="width: 100%; margin-top: 16px;" onclick="openMotorControl()">Manual Motor Control</button>
      <button class="btn-save" onclick="saveSettings()">Save Settings</button>
      <button class="btn-close" onclick="closeSettings()">Close</button>
    </div>
  </div>

  <!-- MOTOR CONTROL PANEL -->
  <div id="motorControlPanel" class="settings-panel">
    <div class="settings-content">
      <h3>Motor Control</h3>
      <div class="lock-indicator" id="lockIndicator">Locked during animations</div>
      <div class="settings-section">
        <div class="motor-controls">
          <div class="motor-slider"><label><span>S0 R1</span><span id="m1val">90&deg;</span></label><input type="range" id="motor1" min="0" max="180" value="90" oninput="updateMotor(1, this.value)"></div>
          <div class="motor-slider"><label><span>S1 R2</span><span id="m2val">90&deg;</span></label><input type="range" id="motor2" min="0" max="180" value="90" oninput="updateMotor(2, this.value)"></div>
          <div class="motor-slider"><label><span>S2 L1</span><span id="m3val">90&deg;</span></label><input type="range" id="motor3" min="0" max="180" value="90" oninput="updateMotor(3, this.value)"></div>
          <div class="motor-slider"><label><span>S3 L2</span><span id="m4val">90&deg;</span></label><input type="range" id="motor4" min="0" max="180" value="90" oninput="updateMotor(4, this.value)"></div>
          <div class="motor-slider"><label><span>S4 R4</span><span id="m5val">90&deg;</span></label><input type="range" id="motor5" min="0" max="180" value="90" oninput="updateMotor(5, this.value)"></div>
          <div class="motor-slider"><label><span>S5 R3</span><span id="m6val">90&deg;</span></label><input type="range" id="motor6" min="0" max="180" value="90" oninput="updateMotor(6, this.value)"></div>
          <div class="motor-slider"><label><span>S6 L3</span><span id="m7val">90&deg;</span></label><input type="range" id="motor7" min="0" max="180" value="90" oninput="updateMotor(7, this.value)"></div>
          <div class="motor-slider"><label><span>S7 L4</span><span id="m8val">90&deg;</span></label><input type="range" id="motor8" min="0" max="180" value="90" oninput="updateMotor(8, this.value)"></div>
        </div>
      </div>
      <button class="btn-close" onclick="closeMotorControl()">Close</button>
    </div>
  </div>

<script>
let isLocked = false;
const termInput = document.getElementById('termInput');
const termOutput = document.getElementById('termOutput');
const termLockBadge = document.getElementById('termLockBadge');
let termHistory = [];
let termHistoryIdx = -1;

termInput.addEventListener('keydown', function(e) {
  if (e.key === 'Enter') {
    const cmd = this.value.trim(); if (!cmd) return;
    termHistory.unshift(cmd); termHistoryIdx = -1;
    appendTermLine('$ ' + cmd, 'cmd-line'); this.value = '';
    sendTerminalCmd(cmd);
  } else if (e.key === 'ArrowUp') {
    e.preventDefault();
    if (termHistoryIdx < termHistory.length - 1) { termHistoryIdx++; this.value = termHistory[termHistoryIdx]; }
  } else if (e.key === 'ArrowDown') {
    e.preventDefault();
    if (termHistoryIdx > 0) { termHistoryIdx--; this.value = termHistory[termHistoryIdx]; }
    else { termHistoryIdx = -1; this.value = ''; }
  }
});

function appendTermLine(text, cls) {
  text.split('\n').forEach(line => { termOutput.innerHTML += '<span class="' + cls + '">' + escapeHtml(line) + '</span><br>'; });
  termOutput.scrollTop = termOutput.scrollHeight;
}
function escapeHtml(str) { return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }

function updateLockBadge(locked) {
  isLocked = locked;
  termLockBadge.textContent = locked ? 'LOCKED' : 'UNLOCKED';
  locked ? termLockBadge.classList.add('locked') : termLockBadge.classList.remove('locked');
}

function sendTerminalCmd(cmd) {
  fetch('/terminal?cmd=' + encodeURIComponent(cmd)).then(r => r.json()).then(data => {
    const response = data.response || 'OK';
    const cmdLower = cmd.toLowerCase();
    let cls = 'response-line';
    if (cmdLower === 'hack') cls = 'hack-line';
    else if (cmdLower === 'muhack') cls = 'muhack-line';
    else if (response.startsWith('ACCESS DENIED') || response.startsWith('Unknown')) cls = 'error-line';
    appendTermLine(response, cls);
    if (typeof data.locked !== 'undefined') updateLockBadge(data.locked);
  }).catch(() => appendTermLine('Connection error', 'error-line'));
}

let commandQueue = 0; const MAX_COMMANDS = 3; let motorsLocked = false;

function updateQueueStatus() {
  const q = document.getElementById('queueStatus');
  q.textContent = 'Command Queue: ' + commandQueue + '/' + MAX_COMMANDS;
  commandQueue >= MAX_COMMANDS ? q.classList.add('full') : q.classList.remove('full');
}
function canSendCommand() { return commandQueue < MAX_COMMANDS; }
function incrementQueue() {
  commandQueue++; updateQueueStatus();
  setTimeout(() => { if (commandQueue > 0) commandQueue--; updateQueueStatus(); }, 1000);
}
function lockMotors(d) {
  d = d || 3000; motorsLocked = true;
  document.getElementById('lockIndicator').classList.add('active');
  for (let i=1;i<=8;i++) { const s=document.getElementById('motor'+i); if(s) s.disabled=true; }
  setTimeout(() => { motorsLocked=false; document.getElementById('lockIndicator').classList.remove('active');
    for (let i=1;i<=8;i++) { const s=document.getElementById('motor'+i); if(s) s.disabled=false; }
  }, d);
}

function move(dir) { if(!canSendCommand())return; incrementQueue(); fetch('/cmd?go='+dir).then(r=>{if(r.status===403)appendTermLine('ACCESS DENIED - Robot is locked','error-line');}).catch(console.log); }
function stop() { commandQueue=0; updateQueueStatus(); fetch('/cmd?stop=1').then(r=>{if(r.status===403)appendTermLine('ACCESS DENIED - Robot is locked','error-line');}).catch(console.log); }
function pose(name) { if(!canSendCommand())return; incrementQueue(); lockMotors(3000); fetch('/cmd?pose='+name).then(r=>{if(r.status===403)appendTermLine('ACCESS DENIED - Robot is locked','error-line');}).catch(console.log); }
function updateMotor(m,v) { if(motorsLocked)return; document.getElementById('m'+m+'val').textContent=v+'\u00B0'; if(!canSendCommand())return; incrementQueue(); fetch('/cmd?motor='+m+'&value='+v).catch(console.log); }

function applyTheme(c) {
  const r=document.documentElement; r.style.setProperty('--accent',c); r.style.setProperty('--content-color',c);
  const rgb=hexToRgb(c); if(rgb){r.style.setProperty('--content-color-dark','rgb('+Math.max(0,rgb.r-20)+','+Math.max(0,rgb.g-20)+','+Math.max(0,rgb.b-20)+')');r.style.setProperty('--content-color-darker','rgb('+Math.max(0,rgb.r-40)+','+Math.max(0,rgb.g-40)+','+Math.max(0,rgb.b-40)+')');}
}
function hexToRgb(h){const r=/^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(h);return r?{r:parseInt(r[1],16),g:parseInt(r[2],16),b:parseInt(r[3],16)}:null;}
function loadTheme(){const c=localStorage.getItem('themeColor');if(c)applyTheme(c);}
document.addEventListener('DOMContentLoaded',loadTheme);

function openSettings() {
  fetch('/getSettings').then(r=>r.json()).then(data=>{
    document.getElementById('frameDelay').value=data.frameDelay||100;
    document.getElementById('walkCycles').value=data.walkCycles||10;
    document.getElementById('motorCurrentDelay').value=data.motorCurrentDelay||20;
    document.getElementById('motorSpeed').value=data.motorSpeed||'medium';
    const sc=localStorage.getItem('themeColor')||'#E63946';const cs=document.getElementById('themeColor');const ci=document.getElementById('customColor');
    let mf=false;for(let o of cs.options){if(o.value===sc){cs.value=sc;mf=true;break;}}
    if(!mf){cs.value='custom';ci.value=sc;ci.style.display='block';}
    document.getElementById('settingsPanel').style.display='block';
  }).catch(()=>{document.getElementById('frameDelay').value=100;document.getElementById('walkCycles').value=10;document.getElementById('motorCurrentDelay').value=20;document.getElementById('settingsPanel').style.display='block';});
  document.getElementById('themeColor').addEventListener('change',function(){const ci=document.getElementById('customColor');if(this.value==='custom'){ci.style.display='block';}else{ci.style.display='none';applyTheme(this.value);}});
  document.getElementById('customColor').addEventListener('input',function(){applyTheme(this.value);});
}
function closeSettings(){document.getElementById('settingsPanel').style.display='none';}
function openMotorControl(){document.getElementById('motorControlPanel').style.display='block';}
function closeMotorControl(){document.getElementById('motorControlPanel').style.display='none';}

function saveSettings(){
  const fd=document.getElementById('frameDelay').value,wc=document.getElementById('walkCycles').value,mcd=document.getElementById('motorCurrentDelay').value,ms=document.getElementById('motorSpeed').value;
  const cs=document.getElementById('themeColor'),ci=document.getElementById('customColor');
  const tc=cs.value==='custom'?ci.value:cs.value;localStorage.setItem('themeColor',tc);applyTheme(tc);
  fetch('/setSettings?frameDelay='+fd+'&walkCycles='+wc+'&motorCurrentDelay='+mcd+'&motorSpeed='+ms).then(()=>closeSettings()).catch(()=>closeSettings());
}

let activeGamepadIndex=null,gamepadPollId=null,lastButtonStates=[],lastAxisDir={x:0,y:0};const axisThreshold=0.5,pollIntervalMs=80;
const buttonBindings={0:()=>pose('stand'),1:()=>pose('wave'),2:()=>pose('dance'),3:()=>pose('swim'),4:()=>pose('point'),5:()=>pose('pushup'),6:()=>pose('bow'),7:()=>pose('shake'),8:()=>stop(),9:()=>pose('rest'),10:()=>pose('cute'),11:()=>pose('freaky'),12:()=>move('forward'),13:()=>move('backward'),14:()=>move('left'),15:()=>move('right'),16:()=>stop(),17:()=>pose('worm')};
const buttonReleaseStop=new Set([12,13,14,15]);
function updateGamepadStatus(c){const s=document.getElementById('gamepadStatus');if(!s)return;s.textContent=c?'Gamepad connected':'Gamepad disconnected';c?s.classList.add('connected'):s.classList.remove('connected');}
function handleButtonChange(i,p){if(p){const a=buttonBindings[i];if(a)a();}else if(buttonReleaseStop.has(i))stop();}
function getAxisDirection(x,y){if(Math.abs(x)<axisThreshold&&Math.abs(y)<axisThreshold)return{x:0,y:0};return Math.abs(x)>Math.abs(y)?{x:x>0?1:-1,y:0}:{x:0,y:y>0?1:-1};}
function applyAxisDirection(d){if(d.x===1)move('right');else if(d.x===-1)move('left');else if(d.y===1)move('backward');else if(d.y===-1)move('forward');else stop();}
function pollGamepad(){const ps=navigator.getGamepads?navigator.getGamepads():[];const p=ps&&activeGamepadIndex!==null?ps[activeGamepadIndex]:null;if(!p){updateGamepadStatus(false);return;}updateGamepadStatus(true);if(!lastButtonStates.length)lastButtonStates=p.buttons.map(b=>!!b.pressed);p.buttons.forEach((b,i)=>{const pr=!!b.pressed;if(pr!==lastButtonStates[i]){handleButtonChange(i,pr);lastButtonStates[i]=pr;}});const x=p.axes[0]||0,y=p.axes[1]||0;const d=getAxisDirection(x,y);if(d.x!==lastAxisDir.x||d.y!==lastAxisDir.y){applyAxisDirection(d);lastAxisDir=d;}}
window.addEventListener('gamepadconnected',(e)=>{activeGamepadIndex=e.gamepad.index;lastButtonStates=[];lastAxisDir={x:0,y:0};updateGamepadStatus(true);if(!gamepadPollId)gamepadPollId=setInterval(pollGamepad,pollIntervalMs);});
window.addEventListener('gamepaddisconnected',(e)=>{if(activeGamepadIndex===e.gamepad.index){activeGamepadIndex=null;lastButtonStates=[];lastAxisDir={x:0,y:0};updateGamepadStatus(false);}});
if(navigator.getGamepads){setInterval(()=>{if(activeGamepadIndex!==null)return;const ps=navigator.getGamepads();if(!ps)return;for(let i=0;i<ps.length;i++){if(ps[i]){activeGamepadIndex=ps[i].index;updateGamepadStatus(true);if(!gamepadPollId)gamepadPollId=setInterval(pollGamepad,pollIntervalMs);break;}}},1000);}
</script>
</body>
</html>
)rawliteral";
