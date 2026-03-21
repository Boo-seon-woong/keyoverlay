# Project: Secure Key Overlay (Tray-Controlled, No-Hook Design)

## Goal

Develop a user-friendly, production-grade Windows key overlay tool with a tray-based control interface.

The application must:

* Avoid antivirus/security conflicts (e.g., AhnLab V3)
* Not capture any keyboard input until explicitly activated by the user
* Provide a clean, intuitive tray-based configuration UI
* Work over fullscreen applications
* Be click-through and non-intrusive

---

## Core Design Philosophy

1. **User-controlled execution**

   * The program MUST NOT capture keyboard input on startup
   * Input capture begins ONLY after user clicks "Start"

2. **Security-first architecture**

   * No low-level hooks
   * Use Raw Input API ONLY when active
   * No background monitoring before activation

3. **Commercial-grade UX**

   * Tray icon as primary interface
   * No config files required
   * All settings accessible via GUI

---

## Application Lifecycle

### State Machine

1. Idle State (default)

   * Tray icon visible
   * No overlay window
   * No input capture
   * No Raw Input registration

2. Config State (via tray)

   * User selects keys
   * User sets position
   * User previews layout

3. Active State (after "Start")

   * Register Raw Input
   * Create overlay window
   * Begin rendering key states

4. Stopped State (after "Stop")

   * Destroy overlay
   * Unregister Raw Input
   * Return to Idle

---

## Tray UI (CRITICAL)

### Tray Icon Menu

Right-click menu:

* Start Overlay
* Stop Overlay
* Settings
* Exit

---

### Settings Window (GUI)

Must be simple and intuitive.

#### Sections:

1. **Key Selection**

   * Checkbox list:

     * W, A, S, D
     * Ctrl, Shift, Alt
     * Optional: custom key add

2. **Position**

   * Presets:

     * Top-left
     * Top-right
     * Bottom-left
     * Bottom-right
   * Custom (x, y)

3. **Appearance**

   * Key size slider
   * Spacing slider
   * Color pickers:

     * Idle
     * Active
     * Text

4. **Preview**

   * Live preview inside settings window
   * DOES NOT use real input

---

## Input Handling

### Activation-only behavior

* Raw Input registration happens ONLY when:
  → user clicks "Start Overlay"

### API Requirements

* Use:

  * RegisterRawInputDevices
  * WM_INPUT

* Do NOT use:

  * SetWindowsHookEx
  * WH_KEYBOARD_LL

---

## Overlay Window

### Properties

* WS_POPUP
* WS_EX_TOPMOST
* WS_EX_LAYERED
* WS_EX_TRANSPARENT

### Behavior

* Always on top
* Click-through (no input blocking)
* No taskbar presence
* No focus stealing

---

## Rendering

* Draw rectangular key boxes
* Display key labels
* State-based coloring:

| State   | Color |
| ------- | ----- |
| Idle    | Gray  |
| Pressed | Green |

* Maintain pressed state while key is held

---

## Key State Management

* Maintain map:

  * key → pressed (bool)

* Update on:

  * WM_INPUT key down/up events

---

## Fullscreen Compatibility

Must:

* Render above DirectX / OpenGL fullscreen apps
* Avoid exclusive fullscreen conflicts

Recommended:

* Use layered window with alpha blending

---

## Performance Constraints

* Event-driven only (no polling)
* Minimal CPU usage
* No heavy frameworks

---

## Technical Stack

* Language: C++
* UI: Win32 API
* Rendering: GDI (or Direct2D if needed)

---

## Security Constraints (IMPORTANT)

* Do not monitor input before user activation
* No persistent background hooks
* No driver interaction
* No suspicious API usage

---

## Deliverables

* Single executable
* No installer required
* Tray icon visible on launch
* Fully functional UI + overlay

---

## Optional Enhancements

* Save/load user settings
* Multiple layout presets
* Mouse click visualization
* Key press animation

---

## Summary

This program must behave like a commercial utility:

* Launch → tray only
* User configures → clicks Start
* Overlay activates safely
* No hidden behavior

Security, UX, and simplicity are the top priorities.
