/* The pure value helpers of CNA's input families (input.h, input_keyboard.h, input_mouse.h, input_gamepad.h,
 * input_touch.h, input_joystick.h, input_haptics.h, input_devices.h), used from C17 without a window: snapshots built
 * from explicit values and queried, hashed and printed. Expectations follow XNA 4.0 (KeyboardState's XOR hash,
 * MouseState's and GamePadState's ToString and hash, TouchLocation's text) and the header documentation. Capturing
 * real device state needs a game of its own and runs in the C game probe instead. */
#include <math.h>

#include "CApi/CProbe.h"
#include "CNA/C/input.h"
#include "CNA/C/input_devices.h"
#include "CNA/C/input_gamepad.h"
#include "CNA/C/input_haptics.h"
#include "CNA/C/input_joystick.h"
#include "CNA/C/input_keyboard.h"
#include "CNA/C/input_mouse.h"
#include "CNA/C/input_touch.h"

#define VERSIONED(value)                              \
    do {                                              \
        memset(&(value), 0, sizeof(value));           \
        (value).struct_size = (uint32_t)sizeof(value); \
        (value).struct_version = 1u;                  \
    } while (0)

static void KeyboardChecks(CProbe* p)
{
    const CNA_Key keys[3] = {CNA_KEY_LEFT_SHIFT, CNA_KEY_A, CNA_KEY_A};
    CNA_KeyboardState state, same, other;
    VERSIONED(state);
    VERSIONED(same);
    VERSIONED(other);
    const CNA_Result created = cna_keyboard_state_init_from_keys(keys, 3u, &state);
    cna_keyboard_state_init_from_keys(keys, 2u, &same);
    cna_keyboard_state_init_from_keys(keys + 1, 1u, &other);
    CNA_Bool aDown = CNA_FALSE, bUp = CNA_FALSE, equal = CNA_FALSE, notEqual = CNA_FALSE;
    cna_keyboard_state_is_key_down(&state, CNA_KEY_A, &aDown);
    cna_keyboard_state_is_key_up(&state, CNA_KEY_B, &bUp);
    CNA_KeyState shiftState = 99;
    cna_keyboard_state_get_key_state(&state, CNA_KEY_LEFT_SHIFT, &shiftState);
    uint32_t count = 0, copiedCount = 0;
    cna_keyboard_state_get_pressed_key_count(&state, &count);
    CNA_Key pressed[4] = {0, 0, 0, 0};
    cna_keyboard_state_copy_pressed_keys(&state, pressed, 4u, &copiedCount);
    cna_keyboard_state_equals(&state, &same, &equal);
    cna_keyboard_state_not_equals(&state, &other, &notEqual);
    int32_t hash = 0;
    cna_keyboard_state_get_hash_code(&state, &hash);
    const CNA_Key outOfRange = 300u;
    CNA_KeyboardState refused;
    VERSIONED(refused);
    const CNA_Result refusedResult = cna_keyboard_state_init_from_keys(&outOfRange, 1u, &refused);
    char text[96];
    uint64_t textBytes = 0;
    cprobe_terminate(text, sizeof text, cna_keyboard_state_copy_string(&state, text, sizeof text - 1, &textBytes), &textBytes);
    cprobe_got(p, "%u: A down %u, B up %u, LeftShift %u, %u pressed [%u, %u], equals %u, not_equals %u, hash %d, key 300 -> %u, \"%s\"", (unsigned)created, aDown, bUp, (unsigned)shiftState,
               (unsigned)count, (unsigned)pressed[0], (unsigned)pressed[1], equal, notEqual, (int)hash, (unsigned)refusedResult, text);
    CPROBE_CHECK(p, "cna_keyboard_state_init_from_keys(LeftShift, A, A) and its queries",
                 "A down, B up, LeftShift DOWN(1), 2 keys in ascending order [65, 160], equal to the same set, hash 3 (XNA: 2 ^ 1, the XOR of the bit words), key 300 refused, text \"Microsoft.Xna.Framework.Input.KeyboardState\"",
                 created == CNA_RESULT_SUCCESS && aDown == CNA_TRUE && bUp == CNA_TRUE && shiftState == CNA_KEY_STATE_DOWN && count == 2u && copiedCount == 2u && pressed[0] == CNA_KEY_A &&
                     pressed[1] == CNA_KEY_LEFT_SHIFT && equal == CNA_TRUE && notEqual == CNA_TRUE && hash == 3 && refusedResult == CNA_RESULT_INVALID_ARGUMENT &&
                     strcmp(text, "Microsoft.Xna.Framework.Input.KeyboardState") == 0);
}

static void MouseChecks(CProbe* p)
{
    CNA_MouseState state, moved, released;
    VERSIONED(state);
    VERSIONED(moved);
    VERSIONED(released);
    cna_mouse_state_init_from_values(10, 20, 120, CNA_MOUSE_BUTTON_LEFT, &state);
    cna_mouse_state_init_from_values(11, 20, 120, CNA_MOUSE_BUTTON_LEFT, &moved);
    cna_mouse_state_init_from_values(10, 20, 120, 0u, &released);
    char text[96];
    uint64_t bytes = 0, sized = 0;
    cna_mouse_state_get_string_size(&state, &sized);
    cprobe_terminate(text, sizeof text, cna_mouse_state_copy_string(&state, text, sizeof text - 1, &bytes), &bytes);
    int32_t hash = 0, releasedHash = 1, movedHash = 0;
    cna_mouse_state_get_hash_code(&state, &hash);
    cna_mouse_state_get_hash_code(&released, &releasedHash);
    cna_mouse_state_get_hash_code(&moved, &movedHash);
    CNA_Bool equal = CNA_TRUE, notEqual = CNA_FALSE;
    cna_mouse_state_equals(&state, &moved, &equal);
    cna_mouse_state_not_equals(&state, &moved, &notEqual);
    cprobe_got(p, "hash %d, without the button %d, at x = 11 %d; equals(x+1) %u, not_equals %u", (int)hash, (int)releasedHash, (int)movedHash, equal, notEqual);
    CPROBE_CHECK(p, "cna_mouse_state_init_from_values(10, 20, 120, LEFT): get_hash_code, equals, not_equals",
                 "the header's hash of position and vertical wheel only: the same without the button, different at x = 11; not equal to x = 11",
                 hash == releasedHash && hash != movedHash && equal == CNA_FALSE && notEqual == CNA_TRUE);
    cprobe_got(p, "\"%s\" (%llu bytes)", text, (unsigned long long)sized);
    CPROBE_CHECK(p, "cna_mouse_state_get_string_size/copy_string(10, 20, 120, LEFT)", "\"{X:10 Y:20 Buttons:Left Wheel:120}\" (34 bytes), XNA 4.0 MouseState.ToString",
                 strcmp(text, "{X:10 Y:20 Buttons:Left Wheel:120}") == 0 && sized == 34u);
}

static void GamePadChecks(CProbe* p)
{
    CNA_GamePadButtonFlags buttons = 0;
    const CNA_Result maskResult = cna_gamepad_buttons_init_from_mask(CNA_GAMEPAD_BUTTON_A | CNA_GAMEPAD_BUTTON_B, &buttons);
    CNA_Bool aPressed = CNA_FALSE, xPressed = CNA_TRUE;
    cna_gamepad_buttons_is_pressed(buttons, CNA_GAMEPAD_BUTTON_A, &aPressed);
    cna_gamepad_buttons_is_pressed(buttons, CNA_GAMEPAD_BUTTON_X, &xPressed);
    CNA_GamePadButtonFlags rejected = 0x1234u;
    const CNA_Result badMask = cna_gamepad_buttons_init_from_mask(0x80000000u, &rejected);
    const CNA_Vector2 left = {2.0f, -0.5f}, right = {0.0f, -3.0f};
    CNA_GamePadThumbSticks sticks;
    memset(&sticks, 0, sizeof sticks);
    cna_gamepad_thumb_sticks_init_from_positions(&left, &right, &sticks);
    CNA_GamePadTriggers triggers;
    memset(&triggers, 0, sizeof triggers);
    cna_gamepad_triggers_init_from_positions(1.5f, 0.05f, &triggers);
    cprobe_got(p, "mask %u -> 0x%X, A %u, X %u, bit 31 -> %u (output %s); sticks (%g, %g) (%g, %g); triggers %g, %g", (unsigned)maskResult, (unsigned)buttons, aPressed, xPressed,
               (unsigned)badMask, rejected == 0x1234u ? "untouched" : "written", (double)sticks.left.x, (double)sticks.left.y, (double)sticks.right.x, (double)sticks.right.y,
               (double)triggers.left, (double)triggers.right);
    CPROBE_CHECK(p, "cna_gamepad_buttons_init_from_mask/is_pressed, thumb_sticks_init_from_positions, triggers_init_from_positions",
                 "A|B = 0x3000, A pressed, X not, bit 31 refused untouched; square clamp (1, -0.5) (0, -1); triggers clamped to 1 and kept at 0.05",
                 maskResult == CNA_RESULT_SUCCESS && buttons == 0x3000u && aPressed == CNA_TRUE && xPressed == CNA_FALSE && badMask == CNA_RESULT_INVALID_ARGUMENT && rejected == 0x1234u &&
                     sticks.left.x == 1.0f && sticks.left.y == -0.5f && sticks.right.x == 0.0f && sticks.right.y == -1.0f && triggers.left == 1.0f && fabsf(triggers.right - 0.05f) < 1e-7f);

    /* The connected snapshot derives the trigger bit past the threshold; XNA's ToString is "{IsConnected:True}". */
    const CNA_Vector2 still = {0.0f, 0.0f};
    CNA_GamePadThumbSticks centred;
    cna_gamepad_thumb_sticks_init_from_positions(&still, &still, &centred);
    CNA_GamePadTriggers pulled;
    cna_gamepad_triggers_init_from_positions(0.0f, 0.9f, &pulled);
    CNA_GamePadState state;
    VERSIONED(state);
    const CNA_Result stateResult = cna_gamepad_state_init_from_components(&centred, &pulled, CNA_GAMEPAD_BUTTON_A, CNA_GAMEPAD_BUTTON_DPAD_UP, &state);
    CNA_GamePadButtonFlags derived = 0;
    cna_gamepad_state_get_buttons(&state, &derived);
    CNA_Bool aAndTrigger = CNA_FALSE, b = CNA_TRUE;
    cna_gamepad_state_is_button_down(&state, CNA_GAMEPAD_BUTTON_A | CNA_GAMEPAD_BUTTON_RIGHT_TRIGGER, &aAndTrigger);
    cna_gamepad_state_is_button_up(&state, CNA_GAMEPAD_BUTTON_B, &b);
    char text[96];
    uint64_t textBytes = 0;
    cprobe_terminate(text, sizeof text, cna_gamepad_state_copy_string(&state, text, sizeof text - 1, &textBytes), &textBytes);
    cprobe_got(p, "%u: connected %u, buttons 0x%X, A+RightTrigger down %u, B up %u", (unsigned)stateResult, state.is_connected, (unsigned)derived, aAndTrigger, b);
    CPROBE_CHECK(p, "cna_gamepad_state_init_from_components, get_buttons, is_button_down/is_button_up",
                 "connected; A, DPadUp and the derived RightTrigger bit (0.9 > 30/255); B up",
                 stateResult == CNA_RESULT_SUCCESS && state.is_connected == CNA_TRUE && (derived & CNA_GAMEPAD_BUTTON_RIGHT_TRIGGER) != 0u && (derived & CNA_GAMEPAD_BUTTON_A) != 0u &&
                     (derived & CNA_GAMEPAD_BUTTON_DPAD_UP) != 0u && aAndTrigger == CNA_TRUE && b == CNA_TRUE);
    cprobe_got(p, "\"%s\"", text);
    CPROBE_CHECK(p, "cna_gamepad_state_copy_string of a connected snapshot", "\"{IsConnected:True}\", XNA 4.0 GamePadState.ToString (it overrides ToString)", strcmp(text, "{IsConnected:True}") == 0);

    /* Dead zones: independent axes zero a small component and rescale the rest; none passes values through. */
    CNA_GamePadAnalogState raw, independent, none;
    memset(&raw, 0, sizeof raw);
    raw.left_thumb_stick.x = 0.1f;
    raw.left_thumb_stick.y = 0.9f;
    raw.right_trigger = 0.5f;
    memset(&independent, 0, sizeof independent);
    memset(&none, 0, sizeof none);
    cna_gamepad_apply_dead_zone(CNA_GAMEPAD_DEAD_ZONE_INDEPENDENT_AXES, &raw, &independent);
    cna_gamepad_apply_dead_zone(CNA_GAMEPAD_DEAD_ZONE_NONE, &raw, &none);
    CNA_GamePadAnalogState untouched;
    memset(&untouched, 0x5A, sizeof untouched);
    const CNA_Result badMode = cna_gamepad_apply_dead_zone(7u, &raw, &untouched);
    const float deadZone = 7849.0f / 32767.0f;
    const float expectedY = (0.9f - deadZone) / (1.0f - deadZone);
    cprobe_got(p, "independent (%.4f, %.4f), none (%.4f, %.4f), mode 7 -> %u", (double)independent.left_thumb_stick.x, (double)independent.left_thumb_stick.y,
               (double)none.left_thumb_stick.x, (double)none.left_thumb_stick.y, (unsigned)badMode);
    CPROBE_CHECK(p, "cna_gamepad_apply_dead_zone(INDEPENDENT_AXES / NONE) on the left stick (0.1, 0.9)",
                 "(0, 0.8685): XNA's left-stick dead zone 7849/32767 removed and the rest rescaled; NONE keeps (0.1, 0.9); an unknown mode -> INVALID_ARGUMENT",
                 independent.left_thumb_stick.x == 0.0f && fabsf(independent.left_thumb_stick.y - expectedY) < 2e-3f && fabsf(none.left_thumb_stick.x - 0.1f) < 1e-6f &&
                     fabsf(none.left_thumb_stick.y - 0.9f) < 1e-6f && badMode == CNA_RESULT_INVALID_ARGUMENT);
}

static void TouchChecks(CProbe* p)
{
    const CNA_Vector2 at = {10.0f, 20.0f}, before = {8.0f, 18.0f};
    CNA_TouchLocation pressed, moved, previous;
    memset(&pressed, 0, sizeof pressed);
    memset(&moved, 0, sizeof moved);
    memset(&previous, 0, sizeof previous);
    cna_touch_location_init_from_values(7, CNA_TOUCH_LOCATION_PRESSED, at, &pressed);
    cna_touch_location_init_with_previous(9, CNA_TOUCH_LOCATION_MOVED, at, CNA_TOUCH_LOCATION_PRESSED, before, &moved);
    CNA_Bool hasPrevious = CNA_TRUE, movedHasPrevious = CNA_FALSE;
    CNA_TouchLocation none;
    cna_touch_location_try_get_previous(&pressed, &none, &hasPrevious);
    cna_touch_location_try_get_previous(&moved, &previous, &movedHasPrevious);
    char text[64];
    uint64_t bytes = 0;
    cprobe_terminate(text, sizeof text, cna_touch_location_copy_string(&pressed, text, sizeof text - 1, &bytes), &bytes);
    int32_t hash = 0;
    cna_touch_location_get_hash_code(&pressed, &hash);
    const int32_t expectedHash = cprobe_hash_add(cprobe_hash_add(7, cprobe_float_hash(10.0f)), cprobe_float_hash(20.0f));
    const CNA_TouchLocation both[2] = {pressed, moved};
    CNA_TouchState state;
    VERSIONED(state);
    cna_touch_state_init_from_locations(both, 2u, &state);
    CNA_TouchLocation found;
    CNA_Bool hit = CNA_FALSE, miss = CNA_TRUE;
    cna_touch_state_find_by_id(&state, 9, &found, &hit);
    CNA_TouchLocation lost;
    cna_touch_state_find_by_id(&state, 42, &lost, &miss);
    cprobe_got(p, "\"%s\", hash %d; pressed has previous %u, moved previous %u at (%g, %g) state %u; state %u touches, find 9 %u (id %d), find 42 %u", text, (int)hash, hasPrevious,
               movedHasPrevious, (double)previous.position.x, (double)previous.position.y, (unsigned)previous.state, (unsigned)state.touch_count, hit, (int)found.id, miss);
    CPROBE_CHECK(p, "cna_touch_location_init_from_values/init_with_previous/try_get_previous/copy_string/get_hash_code, cna_touch_state_init_from_locations/find_by_id",
                 "\"{Position:{X:10 Y:20}}\", hash id + bits(10) + bits(20) as XNA; no previous for a fresh press; the moved touch's previous is PRESSED at (8, 18); id 9 found, 42 not",
                 strcmp(text, "{Position:{X:10 Y:20}}") == 0 && hash == expectedHash && hasPrevious == CNA_FALSE && movedHasPrevious == CNA_TRUE && previous.position.x == 8.0f &&
                     previous.position.y == 18.0f && previous.state == CNA_TOUCH_LOCATION_PRESSED && state.touch_count == 2u && hit == CNA_TRUE && found.id == 9 && miss == CNA_FALSE);
}

static void DescriptorChecks(CProbe* p)
{
    CNA_JoystickInfo joystick, otherJoystick;
    VERSIONED(joystick);
    VERSIONED(otherJoystick);
    joystick.id = 99u;
    cna_joystick_info_init(&joystick);
    cna_joystick_info_init(&otherJoystick);
    CNA_Bool sameName = CNA_FALSE, differentName = CNA_TRUE;
    cna_joystick_info_equals(&joystick, cprobe_view("pad"), &otherJoystick, cprobe_view("pad"), &sameName);
    cna_joystick_info_equals(&joystick, cprobe_view("pad"), &otherJoystick, cprobe_view("wheel"), &differentName);
    CNA_SensorInfo sensor, otherSensor;
    VERSIONED(sensor);
    VERSIONED(otherSensor);
    cna_sensor_info_init(&sensor);
    cna_sensor_info_init(&otherSensor);
    otherSensor.id = 3u;
    CNA_Bool sensorEqual = CNA_TRUE;
    cna_sensor_info_equals(&sensor, cprobe_view(""), &otherSensor, cprobe_view(""), &sensorEqual);
    CNA_InputDeviceInfo device, otherDevice;
    VERSIONED(device);
    VERSIONED(otherDevice);
    cna_input_device_info_init(&device);
    cna_input_device_info_init(&otherDevice);
    CNA_Bool deviceEqual = CNA_FALSE;
    cna_input_device_info_equals(&device, cprobe_view("kbd"), &otherDevice, cprobe_view("kbd"), &deviceEqual);
    CNA_HapticEffect effect, louder;
    VERSIONED(effect);
    VERSIONED(louder);
    cna_haptic_effect_init(&effect);
    cna_haptic_effect_init(&louder);
    louder.level = 1000;
    CNA_Bool effectsEqual = CNA_TRUE, selfEqual = CNA_FALSE;
    cna_haptic_effect_equals(&effect, NULL, 0u, &louder, NULL, 0u, &effectsEqual);
    cna_haptic_effect_equals(&effect, NULL, 0u, &effect, NULL, 0u, &selfEqual);
    CNA_HapticDirection direction;
    memset(&direction, 0x5A, sizeof direction);
    const CNA_Result directionResult = cna_haptic_direction_init(&direction);
    cprobe_got(p, "joystick id %u type %u, same name %u, other name %u; sensor id %u type %u, id 0 vs 3 %u; input device equal %u; effect type %u level %d, vs level 1000 %u, self %u; direction %u",
               (unsigned)joystick.id, (unsigned)joystick.type, sameName, differentName, (unsigned)sensor.id, (unsigned)sensor.type, sensorEqual, deviceEqual, (unsigned)effect.type,
               (int)effect.level, effectsEqual, selfEqual, (unsigned)directionResult);
    CPROBE_CHECK(p, "cna_joystick_info_init/equals, cna_sensor_info_init/equals, cna_input_device_info_init/equals, cna_haptic_effect_init/equals, cna_haptic_direction_init",
                 "defaults id 0 and UNKNOWN type; equality compares the names and ids; a zeroed CONSTANT effect differs from one with level 1000",
                 joystick.id == 0u && joystick.type == CNA_JOYSTICK_TYPE_UNKNOWN && sameName == CNA_TRUE && differentName == CNA_FALSE && sensor.id == 0u && sensor.type == CNA_SENSOR_TYPE_UNKNOWN &&
                     sensorEqual == CNA_FALSE && deviceEqual == CNA_TRUE && effect.type == CNA_HAPTIC_EFFECT_TYPE_CONSTANT && effect.level == 0 && effectsEqual == CNA_FALSE &&
                     selfEqual == CNA_TRUE && directionResult == CNA_RESULT_SUCCESS);
}

void museum_c_probe_input(CProbe* probe)
{
    KeyboardChecks(probe);
    MouseChecks(probe);
    GamePadChecks(probe);
    TouchChecks(probe);
    DescriptorChecks(probe);
}
