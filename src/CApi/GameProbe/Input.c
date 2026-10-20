/* The C game probe's input, devices and sensors with a window of its own: captured keyboard, mouse, gamepad and touch
 * state, the touch panel's gesture queue, text-input, mouse-click and hot-plug events raised through the ABI's test
 * hooks, cursors, haptics and joysticks (input*.h), the clipboard, the device extensions' deterministic test backends
 * (vibration, message boxes, file dialogs, tray, camera) and the host queries (devices.h), and sensors driven by
 * synthetic readings (sensors.h). Nothing here waits for a person or opens real hardware. */
#include <math.h>
#include <stdio.h>
#include <unistd.h>

#include "CApi/GameProbe/GameProbe.h"

static int clicks, clickButton, joystickEvents, joystickId, mouseEvents, mouseId, textUnits, trayClicks, dialogCalls;
static uint16_t lastUnit;
static char dialogPath[256];

static void OnClick(int32_t button, void* context)
{
    (void)context;
    clicks += 1;
    clickButton = button;
}

static void OnJoystick(uint32_t id, void* context)
{
    (void)context;
    joystickEvents += 1;
    joystickId = (int)id;
}

static void OnMouseDevice(uint32_t id, void* context)
{
    (void)context;
    mouseEvents += 1;
    mouseId = (int)id;
}

static void OnText(uint16_t unit, void* context)
{
    (void)context;
    textUnits += 1;
    lastUnit = unit;
}

static void OnTray(void* context)
{
    (void)context;
    trayClicks += 1;
}

static void OnDialog(const CNA_StringView* files, uint64_t count, void* context)
{
    (void)context;
    dialogCalls += 1;
    if (count > 0 && files[0].byte_length < sizeof dialogPath) {
        memcpy(dialogPath, files[0].data, (size_t)files[0].byte_length);
        dialogPath[files[0].byte_length] = '\0';
    }
}

static void CapturedState(GameProbe* g, CNA_Handle game)
{
    CProbe* p = &g->probe;
    CNA_KeyboardState keyboard;
    VERSIONED(keyboard);
    const CNA_Result keyboardResult = cna_keyboard_get_state(game, &keyboard);
    uint32_t pressed = 99;
    cna_keyboard_state_get_pressed_key_count(&keyboard, &pressed);
    CNA_MouseState mouse;
    VERSIONED(mouse);
    const CNA_Result mouseResult = cna_mouse_get_state(game, &mouse);
    CNA_GamePadState pad;
    VERSIONED(pad);
    const CNA_Result padResult = cna_gamepad_get_state(game, CNA_PLAYER_INDEX_FOUR, &pad);
    CNA_GamePadCapabilities padCapabilities;
    VERSIONED(padCapabilities);
    cna_gamepad_get_capabilities(game, CNA_PLAYER_INDEX_FOUR, &padCapabilities);
    CNA_Bool vibrated = CNA_TRUE;
    cna_gamepad_set_vibration(game, CNA_PLAYER_INDEX_FOUR, 0.5f, 0.5f, &vibrated);
    CNA_TouchState touch;
    VERSIONED(touch);
    const CNA_Result touchResult = cna_touch_get_state(game, &touch);
    CNA_TouchCapabilities touchCapabilities;
    VERSIONED(touchCapabilities);
    cna_touch_get_capabilities(game, &touchCapabilities);
    char keyName[32];
    uint64_t keyNameBytes = 0;
    const CNA_Result keyNamed = cna_keyboard_copy_key_name_ext(game, CNA_KEY_A, keyName, sizeof keyName - 1, &keyNameBytes);
    cprobe_terminate(keyName, sizeof keyName, keyNamed, &keyNameBytes);
    CNA_Key back = 0;
    cna_keyboard_get_key_from_name_ext(game, cprobe_view(keyName), &back);
    cprobe_got(p, "keyboard %u: %u keys down; mouse %u at %d,%d; pad four %u: connected %u, capabilities connected %u, vibration applied %u; touch %u: %u touches, device %u; key A named \"%s\" -> %u",
               (unsigned)keyboardResult, (unsigned)pressed, (unsigned)mouseResult, (int)mouse.x, (int)mouse.y, (unsigned)padResult, pad.is_connected, padCapabilities.is_connected, vibrated,
               (unsigned)touchResult, (unsigned)touch.touch_count, touchCapabilities.is_connected, keyName, (unsigned)back);
    CPROBE_CHECK(p, "cna_keyboard_get_state, cna_mouse_get_state, cna_gamepad_get_state/get_capabilities/set_vibration, cna_touch_get_state/get_capabilities, cna_keyboard_copy_key_name_ext/get_key_from_name_ext",
                 "fresh snapshots on the probe's private display: no key down, no pad in slot four (not connected, vibration not applied), no touch device and no touches; key A's name translates back to A",
                 keyboardResult == CNA_RESULT_SUCCESS && pressed == 0u && mouseResult == CNA_RESULT_SUCCESS && padResult == CNA_RESULT_SUCCESS && pad.is_connected == CNA_FALSE &&
                     padCapabilities.is_connected == CNA_FALSE && vibrated == CNA_FALSE && touchResult == CNA_RESULT_SUCCESS && touch.touch_count == 0u &&
                     touchCapabilities.is_connected == CNA_FALSE && keyNamed == CNA_RESULT_SUCCESS && keyNameBytes > 0u && back == CNA_KEY_A);

    /* The touch panel: settings read back, and a gesture queued through the ABI's hook comes out of ReadGesture unchanged. */
    cna_touch_panel_set_enabled_gestures(game, CNA_GESTURE_TYPE_TAP | CNA_GESTURE_TYPE_HOLD);
    CNA_GestureType enabled = 0;
    cna_touch_panel_get_enabled_gestures(game, &enabled);
    cna_touch_panel_set_display_width(game, 480);
    int32_t width = 0;
    cna_touch_panel_get_display_width(game, &width);
    CNA_GestureSample sample, read, empty;
    VERSIONED(sample);
    VERSIONED(read);
    VERSIONED(empty);
    sample.gesture_type = CNA_GESTURE_TYPE_TAP;
    sample.position = (CNA_Vector2){12.0f, 34.0f};
    sample.timestamp_ticks = 1234;
    CNA_Bool availableBefore = CNA_TRUE, availableAfter = CNA_FALSE, availableEnd = CNA_TRUE;
    cna_touch_panel_get_is_gesture_available(game, &availableBefore);
    const CNA_Result queued = cna_touch_panel_enqueue_gesture_ext(game, &sample);
    cna_touch_panel_get_is_gesture_available(game, &availableAfter);
    const CNA_Result readResult = cna_touch_panel_read_gesture(game, &read);
    cna_touch_panel_get_is_gesture_available(game, &availableEnd);
    const CNA_Result emptyResult = cna_touch_panel_read_gesture(game, &empty);
    cna_touch_panel_set_enabled_gestures(game, 0u);
    cprobe_got(p, "enabled 0x%X, width %d; available %u, enqueue %u, available %u, read %u: type %u at (%g, %g) t %lld, available %u, empty read %u", (unsigned)enabled, (int)width, availableBefore,
               (unsigned)queued, availableAfter, (unsigned)readResult, (unsigned)read.gesture_type, (double)read.position.x, (double)read.position.y, (long long)read.timestamp_ticks,
               availableEnd, (unsigned)emptyResult);
    CPROBE_CHECK(p, "cna_touch_panel_set+get_enabled_gestures/set+get_display_width/get_is_gesture_available/enqueue_gesture_ext/read_gesture",
                 "Tap|Hold = 0x5, width 480; a queued Tap at (12, 34) read back unchanged, IsGestureAvailable true only while it waits; reading an empty queue -> INVALID_STATE",
                 enabled == (CNA_GESTURE_TYPE_TAP | CNA_GESTURE_TYPE_HOLD) && width == 480 && availableBefore == CNA_FALSE && queued == CNA_RESULT_SUCCESS && availableAfter == CNA_TRUE &&
                     readResult == CNA_RESULT_SUCCESS && read.gesture_type == CNA_GESTURE_TYPE_TAP && read.position.x == 12.0f && read.position.y == 34.0f && read.timestamp_ticks == 1234 &&
                     availableEnd == CNA_FALSE && emptyResult == CNA_RESULT_INVALID_STATE);
}

static void Events(GameProbe* g, CNA_Handle game)
{
    CProbe* p = &g->probe;
    CNA_MouseEventRegistrationHandle click = CNA_INVALID_HANDLE;
    CNA_JoystickEventRegistrationHandle joystick = CNA_INVALID_HANDLE;
    CNA_InputDeviceEventRegistrationHandle device = CNA_INVALID_HANDLE;
    CNA_TextInputRegistrationHandle text = CNA_INVALID_HANDLE;
    cna_mouse_subscribe_clicked_ext(OnClick, NULL, &click);
    cna_joysticks_subscribe_connected_ext(OnJoystick, NULL, &joystick);
    cna_input_devices_subscribe_mouse_connected_ext(OnMouseDevice, NULL, &device);
    cna_text_input_subscribe_text_input_ext(OnText, NULL, &text);
    cna_mouse_raise_clicked_ext(game, 3);
    cna_joysticks_raise_connected_ext(game, 7u);
    cna_input_devices_raise_mouse_connected_ext(game, 11u);
    cna_text_input_raise_text_input_ext(game, 0x017Du);
    cna_mouse_unsubscribe_clicked_ext(click);
    cna_joysticks_unsubscribe_ext(joystick);
    cna_input_devices_unsubscribe_ext(device);
    cna_text_input_unsubscribe_ext(text);
    cna_mouse_raise_clicked_ext(game, 4);
    cna_text_input_raise_text_input_ext(game, 'x');
    cprobe_got(p, "click %d (button %d), joystick connected %d (id %d), mouse connected %d (id %d), text %d (U+%04X); after unsubscribing: clicks %d, text %d", clicks, clickButton, joystickEvents,
               joystickId, mouseEvents, mouseId, textUnits, (unsigned)lastUnit, clicks, textUnits);
    CPROBE_CHECK(p, "cna_mouse_subscribe_clicked_ext/raise_clicked_ext/unsubscribe_clicked_ext, cna_joysticks_subscribe_connected_ext/raise_connected_ext/unsubscribe_ext, cna_input_devices_subscribe_mouse_connected_ext/raise_mouse_connected_ext/unsubscribe_ext, cna_text_input_subscribe_text_input_ext/raise_text_input_ext/unsubscribe_ext",
                 "each C handler called once with what was raised (button 3, joystick 7, mouse 11, U+017D); nothing after unsubscribing",
                 clicks == 1 && clickButton == 3 && joystickEvents == 1 && joystickId == 7 && mouseEvents == 1 && mouseId == 11 && textUnits == 1 && lastUnit == 0x017Du);
}

static void CursorsHapticsJoysticks(GameProbe* g, CNA_Handle game, CNA_Handle device)
{
    CProbe* p = &g->probe;
    CNA_MouseCursorHandle arrow = CNA_INVALID_HANDLE, custom = CNA_INVALID_HANDLE;
    const CNA_Result stock = cna_mouse_cursor_get_stock_ext(game, CNA_MOUSE_CURSOR_STOCK_ARROW, &arrow);
    const CNA_Color pixels[16] = {{255, 255, 255, 255}};
    CNA_Handle texture = CNA_INVALID_HANDLE;
    cna_texture2d_create_from_rgba8(device, 4u, 4u, pixels, 16u, &texture);
    const CNA_Result created = cna_mouse_cursor_create_from_texture2d(game, texture, 1, 1, &custom);
    CNA_MouseCursorHandle outside = CNA_INVALID_HANDLE;
    const CNA_Result badOrigin = cna_mouse_cursor_create_from_texture2d(game, texture, 9, 9, &outside);
    const CNA_Result set = cna_mouse_set_cursor_ext(game, custom);
    cna_mouse_set_cursor_ext(game, arrow);
    const CNA_Result disposed = cna_mouse_cursor_dispose(custom);
    const CNA_Result stockDisposed = cna_mouse_cursor_dispose(arrow);
    cna_mouse_cursor_destroy(custom);
    cna_mouse_cursor_destroy(arrow);
    cna_texture2d_destroy(texture);
    CNA_HapticDeviceHandle mouseHaptic = CNA_INVALID_HANDLE;
    const CNA_Result opened = cna_haptics_open_from_mouse(game, &mouseHaptic);
    CNA_Bool isOpen = CNA_TRUE, rumbled = CNA_TRUE;
    cna_haptic_device_get_is_open(mouseHaptic, &isOpen);
    cna_haptic_device_play_rumble(mouseHaptic, 0.5f, 100u, &rumbled);
    const CNA_Result hapticDestroyed = cna_haptic_device_destroy(mouseHaptic);
    uint32_t haptics = 99, joysticks = 99;
    cna_haptics_get_count(game, &haptics);
    cna_joysticks_get_count(game, &joysticks);
    uint32_t mice = 0, keyboards = 0;
    cna_input_devices_get_mouse_count(game, &mice);
    cna_input_devices_get_keyboard_count(game, &keyboards);
    cprobe_got(p, "stock arrow %u, custom cursor %u, hot spot outside %u, set %u, dispose %u (stock %u); mouse haptic %u: open %u, rumble applied %u, destroy %u; %u haptic device(s), %u joystick(s); %u mice, %u keyboards",
               (unsigned)stock, (unsigned)created, (unsigned)badOrigin, (unsigned)set, (unsigned)disposed, (unsigned)stockDisposed, (unsigned)opened, isOpen, rumbled, (unsigned)hapticDestroyed,
               (unsigned)haptics, (unsigned)joysticks, (unsigned)mice, (unsigned)keyboards);
    CPROBE_CHECK(p, "cna_mouse_cursor_get_stock_ext/create_from_texture2d/dispose/destroy, cna_mouse_set_cursor_ext, cna_haptics_open_from_mouse/get_count, cna_haptic_device_get_is_open/play_rumble/destroy, cna_joysticks_get_count, cna_input_devices_get_mouse_count/get_keyboard_count",
                 "the stock arrow and a 4x4 cursor with its hot spot inside (one outside refused); disposal succeeds, a no-op for the stock cursor; the mouse's haptic device opens closed and its rumble is a refused no-op; no haptic devices or joysticks on the private display",
                 stock == CNA_RESULT_SUCCESS && created == CNA_RESULT_SUCCESS && badOrigin == CNA_RESULT_INVALID_ARGUMENT && set == CNA_RESULT_SUCCESS && disposed == CNA_RESULT_SUCCESS &&
                     stockDisposed == CNA_RESULT_SUCCESS && opened == CNA_RESULT_SUCCESS && isOpen == CNA_FALSE && rumbled == CNA_FALSE && hapticDestroyed == CNA_RESULT_SUCCESS &&
                     haptics == 0u && joysticks == 0u);
}

static void Clipboard(GameProbe* g, CNA_Handle game)
{
    CProbe* p = &g->probe;
    /* The visitor's clipboard is put back afterwards. */
    static char saved[4096];
    uint64_t savedBytes = 0;
    CNA_Bool hadText = CNA_FALSE;
    cna_clipboard_get_has_text(game, &hadText);
    const CNA_Result savedResult = hadText == CNA_TRUE ? cna_clipboard_copy_text(game, saved, sizeof saved - 1, &savedBytes) : CNA_RESULT_INVALID_STATE;
    cprobe_terminate(saved, sizeof saved, savedResult, &savedBytes);
    const CNA_Result set = cna_clipboard_set_text(game, cprobe_view("CNA C probe \xC5\xBD"));
    char text[64];
    uint64_t bytes = 0, size = 0;
    cna_clipboard_get_text_size(game, &size);
    const CNA_Result copied = cna_clipboard_copy_text(game, text, sizeof text - 1, &bytes);
    cprobe_terminate(text, sizeof text, copied, &bytes);
    static const char bad[] = {'\xC3', '\x28'};
    CNA_StringView invalid = {bad, 2u};
    const CNA_Result encoding = cna_clipboard_set_text(game, invalid);
    if (savedResult == CNA_RESULT_SUCCESS) cna_clipboard_set_text(game, cprobe_view(saved));
    cprobe_got(p, "set %u, size %llu, \"%s\"; invalid UTF-8 -> %u; previous text %s", (unsigned)set, (unsigned long long)size, text, (unsigned)encoding,
               savedResult == CNA_RESULT_SUCCESS ? "restored" : "none to restore");
    CPROBE_CHECK(p, "cna_clipboard_get_has_text/set_text/get_text_size/copy_text", "the UTF-8 text back byte for byte (14 bytes with the 2-byte Z caron); invalid UTF-8 refused (ENCODING)",
                 set == CNA_RESULT_SUCCESS && size == 14u && strcmp(text, "CNA C probe \xC5\xBD") == 0 && encoding == CNA_RESULT_ENCODING);
}

static void DeviceBackends(GameProbe* g, CNA_Handle game, CNA_Handle device)
{
    CProbe* p = &g->probe;
    cna_vibrate_controller_set_test_backend_ext(game, CNA_TRUE, CNA_TRUE, cprobe_view("probe rumble"));
    CNA_Bool supported = CNA_FALSE;
    cna_vibrate_controller_get_is_supported_ext(game, &supported);
    char rumbleName[32];
    uint64_t rumbleBytes = 0;
    const CNA_Result rumbleNamed = cna_vibrate_controller_copy_device_name_ext(game, rumbleName, sizeof rumbleName - 1, &rumbleBytes);
    cprobe_terminate(rumbleName, sizeof rumbleName, rumbleNamed, &rumbleBytes);
    cna_vibrate_controller_start(game, INT64_C(2500000));
    cna_vibrate_controller_stop(game);
    CNA_VibrationTestLog vibration;
    VERSIONED(vibration);
    cna_vibrate_controller_get_test_log_ext(game, &vibration);
    cna_vibrate_controller_set_test_backend_ext(game, CNA_FALSE, CNA_FALSE, cprobe_view(""));
    cna_message_box_set_test_backend_ext(game, CNA_TRUE, 1);
    const CNA_StringView buttons[2] = {cprobe_view("Yes"), cprobe_view("No")};
    int32_t chosen = -5;
    const CNA_Result shown = cna_message_box_show_ext(game, CNA_MESSAGE_BOX_TYPE_WARNING, cprobe_view("C probe"), cprobe_view("A question nobody has to answer"), buttons, 2u, &chosen);
    CNA_MessageBoxTestLog boxes;
    VERSIONED(boxes);
    cna_message_box_get_test_log_ext(game, &boxes);
    cna_message_box_set_test_backend_ext(game, CNA_FALSE, 0);
    const CNA_StringView answers[1] = {cprobe_view("/museum/probe/picked.png")};
    cna_file_dialog_set_test_backend_ext(game, CNA_TRUE, answers, 1u);
    const CNA_Result dialog = cna_file_dialog_show_open_file_ext(game, OnDialog, NULL, NULL, 0u, cprobe_view(""), CNA_FALSE);
    for (int i = 0; i < 5 && dialogCalls == 0; ++i) cna_framework_dispatcher_update(game);
    cna_file_dialog_set_test_backend_ext(game, CNA_FALSE, NULL, 0u);
    cprobe_got(p, "vibration: supported %u \"%s\", %u start(s) last %lld ticks, %u stop(s); message box %u chose %d, log %u choice call(s) type %u with %u buttons; file dialog %u, %d answer(s) \"%s\"",
               supported, rumbleName, (unsigned)vibration.start_calls, (long long)vibration.last_duration_ticks, (unsigned)vibration.stop_calls, (unsigned)shown, (int)chosen,
               (unsigned)boxes.choice_calls, (unsigned)boxes.last_type, (unsigned)boxes.last_button_count, (unsigned)dialog, dialogCalls, dialogPath);
    CPROBE_CHECK(p, "cna_vibrate_controller_set_test_backend_ext/get_is_supported_ext/copy_device_name_ext/start/stop/get_test_log_ext, cna_message_box_set_test_backend_ext/show_ext/get_test_log_ext, cna_file_dialog_set_test_backend_ext/show_open_file_ext, cna_framework_dispatcher_update",
                 "the test backends answer without a person: rumble supported and named, one 250 ms start and one stop logged; the Warning box with 2 buttons answered 1; the open dialog's C handler receives the one prepared path",
                 supported == CNA_TRUE && strcmp(rumbleName, "probe rumble") == 0 && vibration.start_calls == 1u && vibration.last_duration_ticks == INT64_C(2500000) && vibration.stop_calls == 1u &&
                     shown == CNA_RESULT_SUCCESS && chosen == 1 && boxes.choice_calls == 1u && boxes.last_type == CNA_MESSAGE_BOX_TYPE_WARNING && boxes.last_button_count == 2u &&
                     dialog == CNA_RESULT_SUCCESS && dialogCalls == 1 && strcmp(dialogPath, "/museum/probe/picked.png") == 0);

    CNA_SystemTrayHandle tray = CNA_INVALID_HANDLE;
    const CNA_Result trayCreated = cna_system_tray_create_with_test_backend_ext(game, cprobe_view("C probe"), &tray);
    uint64_t entry = 99;
    cna_system_tray_add_entry(tray, cprobe_view("Checked"), CNA_TRUE, CNA_FALSE, CNA_TRUE, OnTray, NULL, &entry);
    cna_system_tray_set_entry_checked(tray, entry, CNA_TRUE);
    CNA_Bool checked = CNA_FALSE, enabledEntry = CNA_TRUE;
    cna_system_tray_get_entry_checked(tray, entry, &checked);
    cna_system_tray_set_entry_enabled(tray, entry, CNA_FALSE);
    cna_system_tray_get_entry_enabled(tray, entry, &enabledEntry);
    cna_system_tray_set_entry_enabled(tray, entry, CNA_TRUE);
    const CNA_Result clicked = cna_system_tray_click_entry_for_tests_ext(tray, entry);
    const CNA_Result pastLast = cna_system_tray_click_entry_for_tests_ext(tray, entry + 1u);
    cna_system_tray_destroy(tray);
    CNA_CameraHandle camera = CNA_INVALID_HANDLE;
    const CNA_Result cameraCreated = cna_camera_create_with_test_backend_ext(game, &camera);
    CNA_CameraState opening = 99, ready = 99;
    cna_camera_get_state_ext(camera, &opening);
    const CNA_Color frame[4] = {{1, 2, 3, 255}, {4, 5, 6, 255}, {7, 8, 9, 255}, {10, 11, 12, 255}};
    cna_camera_set_test_frame_ext(camera, 2, 2, frame, 4u);
    cna_camera_get_state_ext(camera, &ready);
    int32_t frameWidth = 0, frameHeight = 0;
    cna_camera_get_frame_width_ext(camera, &frameWidth);
    cna_camera_get_frame_height_ext(camera, &frameHeight);
    const CNA_Color blank[4] = {{0, 0, 0, 0}};
    CNA_Handle target = CNA_INVALID_HANDLE, wrongSize = CNA_INVALID_HANDLE;
    cna_texture2d_create_from_rgba8(device, 2u, 2u, blank, 4u, &target);
    cna_texture2d_create_from_rgba8(device, 1u, 1u, blank, 1u, &wrongSize);
    CNA_Bool acquired = CNA_FALSE, acquiredWrong = CNA_TRUE;
    cna_camera_try_acquire_frame_ext(camera, target, &acquired);
    cna_camera_try_acquire_frame_ext(camera, wrongSize, &acquiredWrong);
    CNA_Color got[4];
    memset(got, 0, sizeof got);
    uint64_t gotCount = 0;
    cna_texture2d_get_data_rgba8(target, got, 4u, &gotCount);
    cna_texture2d_destroy(wrongSize);
    cna_texture2d_destroy(target);
    cna_camera_destroy(camera);
    cprobe_got(p, "tray %u: checked %u, disabled %u, click %u -> %d handler call(s), past the last entry %u; camera %u: %u then %u, %dx%d, acquired %u (%s), wrong-size texture acquired %u",
               (unsigned)trayCreated, checked, enabledEntry, (unsigned)clicked, trayClicks, (unsigned)pastLast, (unsigned)cameraCreated, (unsigned)opening, (unsigned)ready, (int)frameWidth,
               (int)frameHeight, acquired, memcmp(got, frame, sizeof frame) == 0 ? "the published pixels" : "other pixels", acquiredWrong);
    CPROBE_CHECK(p, "cna_system_tray_create_with_test_backend_ext/add_entry/set+get_entry_checked/set+get_entry_enabled/click_entry_for_tests_ext/destroy, cna_camera_create_with_test_backend_ext/get_state_ext/set_test_frame_ext/get_frame_width_ext/get_frame_height_ext/try_acquire_frame_ext/destroy",
                 "the entry's check and enabled state read back, a test click calls its C handler once, an index past the last refused; the test camera Opening, then Ready with a 2x2 frame copied exactly into a 2x2 texture and refused for a 1x1 one",
                 trayCreated == CNA_RESULT_SUCCESS && checked == CNA_TRUE && enabledEntry == CNA_FALSE && clicked == CNA_RESULT_SUCCESS && trayClicks == 1 && pastLast == CNA_RESULT_INVALID_ARGUMENT &&
                     cameraCreated == CNA_RESULT_SUCCESS && ready == CNA_CAMERA_STATE_READY && opening != CNA_CAMERA_STATE_READY && frameWidth == 2 && frameHeight == 2 && acquired == CNA_TRUE &&
                     memcmp(got, frame, sizeof frame) == 0 && acquiredWrong == CNA_FALSE);

    int32_t cores = 0, ram = 0;
    cna_system_info_get_logical_cpu_core_count_ext(game, &cores);
    cna_system_info_get_system_ram_megabytes_ext(game, &ram);
    const long online = sysconf(_SC_NPROCESSORS_ONLN);
    float scale = -1.0f;
    cna_display_info_get_content_scale_ext(game, &scale);
    CNA_PowerState power = 99;
    int32_t seconds = -2, percent = -2;
    const CNA_Result powerResult = cna_power_get_info(game, &power, &seconds, &percent);
    uint64_t locales = 0;
    const CNA_Result localeResult = cna_locale_get_preferred_count_ext(game, &locales);
    cprobe_got(p, "%d logical cores (sysconf %ld), %d MB RAM, content scale %g, power %u: state %u, %d s, %d %%; locales %u: %llu", (int)cores, online, (int)ram, (double)scale, (unsigned)powerResult,
               (unsigned)power, (int)seconds, (int)percent, (unsigned)localeResult, (unsigned long long)locales);
    CPROBE_CHECK(p, "cna_system_info_get_logical_cpu_core_count_ext/get_system_ram_megabytes_ext, cna_display_info_get_content_scale_ext, cna_power_get_info, cna_locale_get_preferred_count_ext",
                 "the host's logical core count as sysconf reports it, a positive RAM size and content scale; the power and locale queries answer (-1 where a value is unknown)",
                 cores == (int32_t)online && ram > 0 && scale > 0.0f && powerResult == CNA_RESULT_SUCCESS && seconds >= -1 && percent >= -1 && percent <= 100 && localeResult == CNA_RESULT_SUCCESS);
}

static void Sensors(GameProbe* g, CNA_Handle game)
{
    CProbe* p = &g->probe;
    CNA_Bool platformSupported = CNA_TRUE;
    cna_accelerometer_get_is_supported(game, &platformSupported);
    CNA_AccelerometerHandle accelerometer = CNA_INVALID_HANDLE;
    const CNA_Result created = cna_accelerometer_create(game, &accelerometer);
    CNA_AccelerometerReading unsupportedValue;
    VERSIONED(unsupportedValue);
    const CNA_Result refused = cna_accelerometer_get_current_value(accelerometer, &unsupportedValue);
    cna_accelerometer_set_supported_for_tests_ext(accelerometer, CNA_TRUE);
    /* With no sensor behind it, a real Start fails as the canonical one does, with an error id; the test hook marks it started. */
    const CNA_Result started = cna_accelerometer_start(accelerometer);
    int32_t errorId = 0;
    CNA_Bool hasErrorId = CNA_FALSE;
    cna_sensors_get_last_error_id_ext(&errorId, &hasErrorId);
    const CNA_Result forced = cna_accelerometer_set_started_for_tests_ext(accelerometer, CNA_TRUE);
    cna_accelerometer_inject_synthetic_update_ext(accelerometer, 0.0f, 0.0f, 9.80665f);
    CNA_AccelerometerReading reading;
    VERSIONED(reading);
    const CNA_Result read = cna_accelerometer_get_current_value(accelerometer, &reading);
    CNA_Bool valid = CNA_FALSE;
    cna_accelerometer_get_is_data_valid(accelerometer, &valid);
    cna_accelerometer_set_started_for_tests_ext(accelerometer, CNA_FALSE);
    cna_accelerometer_destroy(accelerometer);
    CNA_CompassHandle compass = CNA_INVALID_HANDLE;
    cna_compass_create(game, &compass);
    cna_compass_set_test_backend_ext(compass, CNA_TRUE, CNA_TRUE);
    cna_compass_start(compass);
    CNA_CompassReading north, current;
    VERSIONED(north);
    VERSIONED(current);
    const CNA_Vector3 field = {20.0f, 0.0f, -40.0f};
    const CNA_DateTimeOffset now = {INT64_C(638000000000000000), 0};
    cna_compass_reading_init_from_values(5.0, 90.0, field, now, 92.5, &north);
    const CNA_Result injected = cna_compass_inject_synthetic_update_ext(compass, &north);
    cna_compass_get_current_value(compass, &current);
    CNA_Bool same = CNA_FALSE;
    cna_compass_reading_equals(&north, &current, &same);
    cna_compass_stop(compass);
    cna_compass_destroy(compass);
    cprobe_got(p, "platform accelerometer %u; create %u, value while unsupported -> %u; forced supported: Start %u (error id %u/%d), forced started %u, inject 9.80665 m/s2 -> read %u (%g, %g, %g) valid %u; compass test backend inject %u, current equals it %u",
               platformSupported, (unsigned)created, (unsigned)refused, (unsigned)started, hasErrorId, (int)errorId, (unsigned)forced, (unsigned)read, (double)reading.acceleration.x, (double)reading.acceleration.y,
               (double)reading.acceleration.z, valid, (unsigned)injected, same);
    CPROBE_CHECK(p, "cna_accelerometer_get_is_supported/create/get_current_value/set_supported_for_tests_ext/start/set_started_for_tests_ext/inject_synthetic_update_ext/get_is_data_valid/destroy, cna_sensors_get_last_error_id_ext, cna_compass_create/set_test_backend_ext/start/inject_synthetic_update_ext/get_current_value/stop/destroy",
                 "no accelerometer on this host, so CurrentValue is refused (INVALID_STATE); a real Start fails (INVALID_STATE) carrying the canonical error id; marked started, 9.80665 m/s2 reads as 1 g, valid; the compass reports the injected reading",
                 platformSupported == CNA_FALSE && created == CNA_RESULT_SUCCESS && refused == CNA_RESULT_INVALID_STATE && started == CNA_RESULT_INVALID_STATE && hasErrorId == CNA_TRUE &&
                     forced == CNA_RESULT_SUCCESS &&
                     read == CNA_RESULT_SUCCESS && fabsf(reading.acceleration.z - 1.0f) < 1e-5f && reading.acceleration.x == 0.0f && valid == CNA_TRUE && injected == CNA_RESULT_SUCCESS &&
                     same == CNA_TRUE);
}

void gp_input(GameProbe* g, CNA_Handle game, CNA_Handle device)
{
    CapturedState(g, game);
    Events(g, game);
    CursorsHapticsJoysticks(g, game, device);
    Clipboard(g, game);
    DeviceBackends(g, game, device);
    Sensors(g, game);
}
