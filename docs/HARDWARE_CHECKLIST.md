# Manual hardware checklist

The automatic checks run on a private virtual display with software rendering, a dummy or disk audio device and no
physical input devices. What they cannot prove is listed here as manual procedures: each names the device, the room,
the steps, what must be observed, and what to keep as evidence. A result is recorded in `plan.md` (item I07/P12) with
the date, the machine, the museum and CNA revisions and the evidence; a step that was not done stays open — "no device
here" is never a pass.

Start the museum normally (`build/bin/cna-3d-museum`), walk to the room (or use the directory, Tab), and use the
panel. Rooms that open something outside the museum do it only from their buttons.

| Device | Room | Steps | Must observe | Evidence |
|---|---|---|---|---|
| Physical GPU (Vulkan/GL driver, not llvmpipe/lavapipe) | floor 9 *Renderer Validation*, all rv-* | `tools/renderer_check.sh OPENGLES3 FNA3D SDL_GPU WEBGPU` with `LIBGL_ALWAYS_SOFTWARE` unset and the real ICD | every check PASS or a named SKIP; the log names the hardware driver | `build/renderer_check/*.checks`, `glxinfo -B` / `vulkaninfo --summary` output |
| Loudspeakers or headphones | au-basics, au-synth, au-generate, au-spatial | play each sound; in au-spatial walk around the emitter | sounds are audible, pan follows the emitter, pitch slider changes pitch, Bell is a clear 880 Hz tone | a short recording or a signed note; `tools/audio_check.sh` already proves the PCM |
| Microphone | au-mic | press start, speak, stop | the level meter moves; BufferReady count rises; stop ends capture | screenshot of the panel, log lines `[museum][audio]` |
| Game pad (Xbox / PlayStation / Switch layout) | in-pad, in-players, in-devices | move sticks and triggers, press every button, plug and unplug | readouts follow, Identity shows type/GUID/label of A, IsConnected toggles on hotplug, vibration felt | screenshot, log |
| Pad with gyro or touchpad | in-pad | tilt, touch the pad | Identity says gyro yes / touchpad fingers > 0 | screenshot |
| Haptic device (force-feedback wheel, pad with effects) | dev-haptics | Open the first device, Rumble, Constant effect, Gain/Pause | rumble felt; "CreateEffectEXT n, update yes, run yes …"; closing stops all effects | log lines `[museum][devices] haptics:` |
| Joystick | in-joystick | move axes, press buttons, hats | capability counts and live state | screenshot |
| Touch screen | in-touch | tap, hold, drag, flick, pinch | gestures listed with positions and deltas | screenshot |
| Camera | dev-camera | Open the default camera (private: only on your own machine) | state Ready, frames counted, picture on the screen; Close turns it off | screenshot; or `MUSEUM_ALLOW_CAMERA=1 tools/devices_check.sh` |
| Desktop with a system tray (StatusNotifier host) | dev-tray | Create a tray icon, click its entries, toggle | icon and menu appear, "tray entry clicked" logged, checkable entry follows | screenshot, log |
| Real file dialogs (portal or native) | dev-dialogs | each dialog button; pick a file, cancel once | callback reports the picked path, cancel reports no file | log lines `[museum][devices] FileDialog…` |
| Message box with a pointer click | dev-dialogs | MessageBox.Show, click "No" | "MessageBox.Show returned button 1 (No)" | log |
| Browser | dev-tray | UrlLauncher.Open | the default viewer opens the local file | log line with `returned true` |
| Phone sensors (Android/iOS build) | in-phone | Start each sensor, move the device | accelerometer/gyro/compass/motion readings change; vibration felt | screenshots per sensor |
| Display hotplug / resize | pl-window | drag a window edge, move to a second screen | ClientSizeChanged counts rise; content scale updates | screenshot |
| Window minimise on a real desktop | dev-window | MinimizeEXT, then RestoreEXT 2 s later | the window disappears and returns (automated on a private display with openbox: `tools/window_check.sh` reads WM_STATE Iconic, then Normal; this row is only the look on a real desktop) | observation note |
| Text drag and drop | dev-window | drag text from another program onto the window | "TextDropEXT: \"…\"" logged (automated with a real XDND source on a private display: `tools/window_check.sh`; this row only repeats it with a desktop program) | log |
| Device loss (driver reset, GPU switch, sleep/resume) | dev-window, rt-* | suspend and resume the machine with the museum open | DeviceLost/DeviceReset counted, render targets report IsContentLost and are re-rendered | log, screenshot |
| Windows (real, not Wine): DIRECTX9 | whole museum | `tools/tour.sh -f 3 -p` from a native Windows build | exit 0, 0 exhibit errors | `build/tour.log` |
| macOS/Metal-backed renderer, Android, iOS | whole museum | platform build and tour | as above | logs |

Physical GPU without a display (2026-10-20): `MUSEUM_GPU=1` makes the tools run the museum with SDL's `offscreen`
video driver, which needs no display server at all (EGL through the GPU's render node, Vulkan through
`VK_EXT_headless_surface`), so the real GPU renders while nothing reaches the live desktop.
`MUSEUM_GPU=1 tools/renderer_check.sh OPENGLES3 OPENGL33` writes `build/renderer_check/<R>-gpu.checks` and, from Mesa's
EGL and the Vulkan loader, `<R>-gpu.device` naming the driver that drew. Measured on the AMD Phoenix GPU (PCI 1002:15bf,
Mesa 25.0.7 radeonsi): OPENGLES3 and OPENGL33 97 PASS, 0 FAIL, 2 INFO, 1 SKIP each, the same set as llvmpipe; the
press-all tour 0 exhibit errors over 316 rooms; every reference view within 1 % of the llvmpipe images. CNA's
`GraphicsAdapter` cannot tell the two apart (like FNA it reports the display's name and the machine's PCI ids even
while Mesa renders in software), so the `.device` file is the evidence, not the museum's adapter line.
The other renderers need a build that contains them (`BUILD_DIR=build-probe`, with
`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/radeon_icd.json` for Vulkan): SDL_GPU on RADV 97 PASS, 0 FAIL, 2 INFO, 1 SKIP
(the Vulkan loader's choice is in the `.device` file); FNA3D on radeonsi 75 PASS, 0 FAIL, 2 INFO, 23 SKIP (the surface
formats FNA3D cannot create, as on llvmpipe). WEBGPU cannot run offscreen (CNA's WebGPU renderer needs a native window
for its surface: "WebGPURenderer::Surface failed: missing native window (Headless)"), so `MUSEUM_GPU=weston` starts a
private headless Weston (`~/deps/weston`, Weston 14 unpacked from Debian's packages) that renders with the GPU (its log
says "GL vendor: AMD") for the run only, and the museum presents to it through Wayland: WEBGPU on "AMD Radeon 780M
(RADV PHOENIX), Vulkan backend" 76 PASS, 0 FAIL, 2 INFO, 22 SKIP (the surface formats WEBGPU cannot create, as on lavapipe).

Blockers in the environment where this museum is developed (2026-10-20):
- **GPU on a virtual display:** Xvfb has no DRI3, so on it OpenGL is software (llvmpipe) and RADV refuses to present
  ("No DRI3 support detected - required for presentation"). Use `MUSEUM_GPU=1` or `MUSEUM_GPU=weston` (above) instead;
  what neither covers is presentation to a real screen (display vsync, mode changes, exclusive fullscreen), which stays a
  manual step.
- no haptic device, no touch screen, no phone sensors, no tray host on the private display, a real camera that is
  private and therefore not opened automatically, no Windows or macOS machine.
