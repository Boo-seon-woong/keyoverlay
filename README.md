#taskkill /IM SecureKeyOverlay.exe /F

# Secure Key Overlay

Windows tray utility that shows selected key presses on a click-through overlay.

## For End Users (No Coding Needed)

1. Double-click `SecureKeyOverlay.exe`.
2. The settings window opens immediately, and a tray icon is added to the taskbar notification area.
3. Choose keys, position, and appearance in the settings window.
4. Click `Start Overlay` in settings, or right-click the tray icon and choose `Start Overlay`.
5. Click `Stop Overlay` in settings, or use the tray icon menu to stop capturing input.
6. Right-click tray icon -> `Exit` to close.

The app does not register Raw Input before `Start Overlay`.
If the Windows tray becomes unavailable, the app exits and closes the overlay.

## Build

Run:

```bash
./build.sh
```

Output:

`dist/SecureKeyOverlay.exe`

The build uses static C++ runtime linking, so the generated executable can be
distributed by itself without bundling `libc++` or `libunwind` DLLs.
