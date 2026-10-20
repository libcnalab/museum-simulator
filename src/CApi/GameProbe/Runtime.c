/* The C game probe's runtime checks: the renderer selection before and after the latch (core_ext.h), the game and its
 * window, timing and launch parameters (runtime.h, runtime_window.h), the graphics device manager configuration
 * (runtime_graphics_manager.h) and a game component written in C (runtime_components.h). */
#include <math.h>
#include <stdio.h>

#include "CApi/GameProbe/GameProbe.h"

void gp_before_game(GameProbe* g)
{
    CProbe* p = &g->probe;
    /* Nothing has created a device in this process yet: the selection is open, and asking for the active renderer is refused. */
    CNA_Bool latched = CNA_TRUE;
    cna_graphics_renderer_get_is_latched_ext(&latched);
    CNA_GraphicsRendererType selected = 0, active = 0, after = 0;
    const CNA_Result selectedResult = cna_graphics_renderer_get_selected_ext(&selected);
    const CNA_Result activeResult = cna_graphics_renderer_get_active_ext(&active);
    /* Re-selecting what is already selected changes nothing (the museum's environment chose it) but must be accepted. */
    const CNA_Result preferred = cna_graphics_renderer_set_preferred_ext(selected);
    cna_graphics_renderer_get_selected_ext(&after);
    const CNA_Result retired = cna_graphics_renderer_set_preferred_ext(1u);
    char message[600];
    cprobe_last_message(message, sizeof message);
    CNA_Bool fallback = CNA_TRUE;
    cna_graphics_renderer_get_automatic_fallback_ext(&fallback);
    uint64_t fallbacks = 99;
    cna_graphics_renderer_get_fallback_count_ext(&fallbacks);
    cprobe_got(p, "latched %u, selected %u (%u), active -> %u, set_preferred(selected) %u -> %u, identity 1 -> %u \"%.80s\", automatic fallback %u, %llu fallbacks", latched, (unsigned)selected,
               (unsigned)selectedResult, (unsigned)activeResult, (unsigned)preferred, (unsigned)after, (unsigned)retired, message, fallback, (unsigned long long)fallbacks);
    CPROBE_CHECK(p, "before any device: cna_graphics_renderer_get_is_latched/get_selected/get_active/set_preferred/get_automatic_fallback/get_fallback_count_ext",
                 "not latched; get_active -> CNA_RESULT_INVALID_STATE; re-selecting the selection succeeds; a retired identity -> INVALID_ARGUMENT naming it; no fallback configured, none taken",
                 latched == CNA_FALSE && selectedResult == CNA_RESULT_SUCCESS && activeResult == CNA_RESULT_INVALID_STATE && preferred == CNA_RESULT_SUCCESS && after == selected &&
                     retired == CNA_RESULT_INVALID_ARGUMENT && message[0] != '\0' && fallback == CNA_FALSE && fallbacks == 0u);
}

static void ComponentInitialize(void* context)
{
    ((GameProbe*)context)->componentInitialized += 1;
}

static void ComponentUpdate(const CNA_GameTime* time, void* context)
{
    (void)time;
    ((GameProbe*)context)->componentUpdates += 1;
}

void gp_runtime(GameProbe* g, CNA_Handle game, CNA_Handle device)
{
    (void)device;
    CProbe* p = &g->probe;
    char text[256];
    uint64_t bytes = 0;

    /* The window: the creation title, a new title, its bounds and type names. */
    char before[64], after[64], gameType[96], windowType[96];
    uint64_t beforeBytes = 0, afterBytes = 0, gameTypeBytes = 0, windowTypeBytes = 0, titleSize = 0;
    const CNA_Result beforeResult = cna_game_window_copy_title(game, before, sizeof before - 1, &beforeBytes);
    cprobe_terminate(before, sizeof before, beforeResult, &beforeBytes);
    const CNA_Result retitled = cna_game_set_window_title(game, cprobe_view("CNA C game probe, frame 1"));
    cna_game_window_get_title_size(game, &titleSize);
    const CNA_Result afterResult = cna_game_window_copy_title(game, after, sizeof after - 1, &afterBytes);
    cprobe_terminate(after, sizeof after, afterResult, &afterBytes);
    CNA_Rectangle bounds = {0, 0, 0, 0};
    cna_game_window_get_client_bounds(game, &bounds);
    const CNA_Result gameTypeResult = cna_game_copy_type_name(game, gameType, sizeof gameType - 1, &gameTypeBytes);
    cprobe_terminate(gameType, sizeof gameType, gameTypeResult, &gameTypeBytes);
    const CNA_Result windowTypeResult = cna_game_window_copy_type_name(game, windowType, sizeof windowType - 1, &windowTypeBytes);
    cprobe_terminate(windowType, sizeof windowType, windowTypeResult, &windowTypeBytes);
    static const char nul[] = {'a', '\0', 'b'};
    CNA_StringView embedded = {nul, 3u};
    const CNA_Result embeddedResult = cna_game_set_window_title(game, embedded);
    cprobe_got(p, "title \"%s\" -> %u \"%s\" (%llu bytes); client bounds %dx%d; game \"%s\", window \"%s\"; embedded NUL -> %u", before, (unsigned)retitled, after,
               (unsigned long long)titleSize, (int)bounds.width, (int)bounds.height, gameType, windowType, (unsigned)embeddedResult);
    CPROBE_CHECK(p, "cna_game_window_copy_title/get_title_size, cna_game_set_window_title, cna_game_window_get_client_bounds, cna_game_copy_type_name, cna_game_window_copy_type_name",
                 "the creation title, then the new one (25 bytes); a non-empty client area; \"Microsoft.Xna.Framework.Game\" and \"Microsoft.Xna.Framework.GameWindow\"; a title with an embedded NUL refused (ENCODING)",
                 strcmp(before, "CNA C game probe") == 0 && retitled == CNA_RESULT_SUCCESS && strcmp(after, "CNA C game probe, frame 1") == 0 && titleSize == 25u && bounds.width > 0 &&
                     bounds.height > 0 && strcmp(gameType, "Microsoft.Xna.Framework.Game") == 0 && strcmp(windowType, "Microsoft.Xna.Framework.GameWindow") == 0 &&
                     embeddedResult == CNA_RESULT_ENCODING);

    /* Timing: the step given at creation, a new step and what it implies, and XNA's frame-rate arithmetic. */
    int64_t ticks = 0, newTicks = 0;
    CNA_Bool fixed = CNA_TRUE;
    cna_game_get_target_elapsed_time_ticks(game, &ticks);
    cna_game_get_is_fixed_time_step(game, &fixed);
    cna_game_set_target_elapsed_time_ticks(game, INT64_C(333333));
    cna_game_get_target_elapsed_time_ticks(game, &newTicks);
    double fps = 0.0, frameMs = 0.0, sixty = 0.0;
    cna_game_get_target_fps_ext(game, &fps);
    cna_game_get_target_ms_frame_time_ext(game, &frameMs);
    cna_game_fps_to_milliseconds_per_frame_ext(60, &sixty);
    const CNA_Result zeroStep = cna_game_set_target_elapsed_time_ticks(game, 0);
    cna_game_set_target_elapsed_time_ticks(game, INT64_C(166667));
    cprobe_got(p, "created step %lld, fixed %u; set 333333 -> %lld, %.4f fps, %.4f ms; 60 fps -> %.6f ms; zero step -> %u", (long long)ticks, fixed, (long long)newTicks, fps, frameMs, sixty,
               (unsigned)zeroStep);
    CPROBE_CHECK(p, "cna_game_get/set_target_elapsed_time_ticks, get_is_fixed_time_step, get_target_fps_ext, get_target_ms_frame_time_ext, fps_to_milliseconds_per_frame_ext",
                 "166667 and variable as created; 333333 ticks = 30.0000 fps = 33.3333 ms; 60 fps = 16.666667 ms; a zero step refused (INVALID_ARGUMENT, XNA requires a positive TargetElapsedTime)",
                 ticks == INT64_C(166667) && fixed == CNA_FALSE && newTicks == INT64_C(333333) && fabs(fps - 30.0) < 1e-3 && fabs(frameMs - 33.3333) < 1e-3 && fabs(sixty - 1000.0 / 60.0) < 1e-6 &&
                     zeroStep == CNA_RESULT_INVALID_ARGUMENT);

    /* Launch parameters: add, find, read back, refuse a duplicate. */
    uint64_t countBefore = 99, countAfter = 0;
    cna_game_launch_parameters_get_count(game, &countBefore);
    const CNA_Result added = cna_game_launch_parameters_add(game, cprobe_view("level"), cprobe_view("3"));
    const CNA_Result duplicate = cna_game_launch_parameters_add(game, cprobe_view("level"), cprobe_view("9"));
    CNA_Bool present = CNA_FALSE;
    cna_game_launch_parameters_contains_key(game, cprobe_view("level"), &present);
    cna_game_launch_parameters_get_count(game, &countAfter);
    const CNA_Result valueResult = cna_game_launch_parameters_copy_value(game, cprobe_view("level"), text, sizeof text - 1, &bytes);
    cprobe_terminate(text, sizeof text, valueResult, &bytes);
    /* The key is found among all the keys (the collection's order is its own). */
    char key[64];
    key[0] = '\0';
    for (uint64_t i = 0; i < countAfter; ++i) {
        char candidate[64];
        uint64_t candidateBytes = 0;
        const CNA_Result keyResult = cna_game_launch_parameters_copy_key(game, i, candidate, sizeof candidate - 1, &candidateBytes);
        cprobe_terminate(candidate, sizeof candidate, keyResult, &candidateBytes);
        if (strcmp(candidate, "level") == 0) snprintf(key, sizeof key, "%s", candidate);
    }
    cprobe_got(p, "count %llu -> add %u, duplicate %u, contains %u, count %llu, value \"%s\", key listed \"%s\"", (unsigned long long)countBefore, (unsigned)added, (unsigned)duplicate, present,
               (unsigned long long)countAfter, text, key);
    CPROBE_CHECK(p, "cna_game_launch_parameters_get_count/add/contains_key/copy_value/copy_key", "one more parameter \"level\" = \"3\"; the duplicate refused (INVALID_ARGUMENT) and the first value kept",
                 added == CNA_RESULT_SUCCESS && duplicate == CNA_RESULT_INVALID_ARGUMENT && present == CNA_TRUE && countAfter == countBefore + 1u && strcmp(text, "3") == 0 &&
                     strcmp(key, "level") == 0);

    /* Title location: the title container reads a museum file by a name relative to it, exactly as the file system has it,
       and refuses an absolute name as XNA's TitleContainer.OpenStream does (ArgumentException). */
    char basePath[1024];
    uint64_t baseBytes = 0;
    const CNA_Result baseResult = cna_title_location_copy_path(game, basePath, sizeof basePath - 1, &baseBytes);
    cprobe_terminate(basePath, sizeof basePath, baseResult, &baseBytes);
    char absolute[1200], relative[1200];
    snprintf(absolute, sizeof absolute, "%s/Fonts/ui.txt", g->contentRoot);
    const size_t baseLength = strlen(basePath);
    const int underTitle = baseLength > 0 && strncmp(absolute, basePath, baseLength) == 0;
    snprintf(relative, sizeof relative, "%s", underTitle ? absolute + baseLength + (absolute[baseLength] == '/' ? 1 : 0) : "Content/Fonts/ui.txt");
    static uint8_t viaTitle[65536], viaFile[65536];
    uint64_t titleBytes = 0, absoluteBytes = 0;
    const CNA_Result titleRead = cna_title_container_read_ext(game, cprobe_view(relative), viaTitle, sizeof viaTitle, &titleBytes);
    const CNA_Result absoluteRead = cna_title_container_read_ext(game, cprobe_view(absolute), NULL, 0u, &absoluteBytes);
    size_t fileBytes = 0;
    FILE* file = fopen(absolute, "rb");
    if (file != NULL) {
        fileBytes = fread(viaFile, 1, sizeof viaFile, file);
        fclose(file);
    }
    uint64_t missingBytes = 0;
    const CNA_Result missing = cna_title_container_read_ext(game, cprobe_view("no-such-file.bin"), viaTitle + 0, 0u, &missingBytes);
    cprobe_got(p, "base path %u (%llu bytes), content %s it; \"%s\" %u: %llu bytes via TitleContainer, %zu via fopen, %s; the absolute name -> %u; a missing file -> %u", (unsigned)baseResult,
               (unsigned long long)baseBytes, underTitle ? "under" : "not under", relative, (unsigned)titleRead, (unsigned long long)titleBytes, fileBytes,
               titleBytes == fileBytes && memcmp(viaTitle, viaFile, fileBytes) == 0 ? "identical" : "different", (unsigned)absoluteRead, (unsigned)missing);
    CPROBE_CHECK(p, "cna_title_location_copy_path, cna_title_container_read_ext",
                 "the museum's Content/Fonts/ui.txt read whole by its name relative to the title path and byte-identical to the file; an absolute name refused (INVALID_ARGUMENT, XNA's ArgumentException); a missing file -> CNA_RESULT_IO",
                 baseResult == CNA_RESULT_SUCCESS && baseBytes > 0u && underTitle && titleRead == CNA_RESULT_SUCCESS && fileBytes > 0u && titleBytes == fileBytes &&
                     memcmp(viaTitle, viaFile, fileBytes) == 0 && absoluteRead == CNA_RESULT_INVALID_ARGUMENT && missing == CNA_RESULT_IO);

    /* The graphics device manager: XNA's GraphicsDeviceInformation defaults, and the one manager a game may have. */
    CNA_GraphicsDeviceInformation information, clone;
    VERSIONED(information);
    VERSIONED(clone);
    cna_graphics_device_information_init(&information);
    information.presentation_parameters.back_buffer_width = 320;
    cna_graphics_device_information_clone(&information, &clone);
    char informationType[96];
    uint64_t informationTypeBytes = 0;
    const CNA_Result informationTypeResult = cna_graphics_device_information_copy_type_name(informationType, sizeof informationType - 1, &informationTypeBytes);
    cprobe_terminate(informationType, sizeof informationType, informationTypeResult, &informationTypeBytes);
    CNA_Bool hadManager = CNA_FALSE, hasManager = CNA_FALSE;
    cna_game_services_contains_ext(game, CNA_GAME_SERVICE_TYPE_GRAPHICS_DEVICE_MANAGER, &hadManager);
    CNA_GraphicsDeviceManagerHandle manager = CNA_INVALID_HANDLE;
    const CNA_Result managerResult = cna_graphics_device_manager_create(game, &manager);
    cna_game_services_contains_ext(game, CNA_GAME_SERVICE_TYPE_GRAPHICS_DEVICE_MANAGER, &hasManager);
    CNA_GraphicsProfile profile = 99;
    int32_t width = 0;
    if (managerResult == CNA_RESULT_SUCCESS) {
        g->deviceManager = manager;
        cna_graphics_device_manager_get_graphics_profile(manager, &profile);
        cna_graphics_device_manager_set_preferred_back_buffer_width(manager, 640);
        cna_graphics_device_manager_get_preferred_back_buffer_width(manager, &width);
    }
    cprobe_got(p, "information: adapter %d, profile %u, format %u; clone width %d; \"%s\"; manager before %u, create %u, after %u; profile %u, preferred width %d",
               (int)information.adapter_index, (unsigned)information.graphics_profile, (unsigned)information.presentation_parameters.back_buffer_format,
               (int)clone.presentation_parameters.back_buffer_width, informationType, hadManager, (unsigned)managerResult, hasManager, (unsigned)profile, (int)width);
    CPROBE_CHECK(p, "cna_graphics_device_information_init/clone/copy_type_name, cna_game_services_contains_ext, cna_graphics_device_manager_create/get_graphics_profile/set+get_preferred_back_buffer_width",
                 "no adapter (-1), Reach (XNA's default profile), Color; the clone keeps width 320; \"Microsoft.Xna.Framework.GraphicsDeviceInformation\"; a game without a manager gets one and then reports it, a game with one refuses a second (INVALID_ARGUMENT)",
                 information.adapter_index == -1 && information.graphics_profile == CNA_GRAPHICS_PROFILE_REACH && information.presentation_parameters.back_buffer_format == CNA_SURFACE_FORMAT_COLOR && clone.presentation_parameters.back_buffer_width == 320 &&
                     strcmp(informationType, "Microsoft.Xna.Framework.GraphicsDeviceInformation") == 0 &&
                     (hadManager == CNA_TRUE ? managerResult == CNA_RESULT_INVALID_ARGUMENT : (managerResult == CNA_RESULT_SUCCESS && hasManager == CNA_TRUE && width == 640)));

    /* A component written in C: created, added, ordered; the game initialises it and updates it every frame (counted at the end). */
    CNA_GameComponentCallbacks callbacks;
    VERSIONED(callbacks);
    cna_game_component_callbacks_init(&callbacks);
    callbacks.initialize = ComponentInitialize;
    callbacks.update = ComponentUpdate;
    callbacks.context = g;
    CNA_GameComponentHandle component = CNA_INVALID_HANDLE;
    const CNA_Result componentResult = cna_game_component_create(game, &callbacks, &component);
    uint64_t components = 99, componentsAfter = 0;
    cna_game_components_get_count(game, &components);
    const CNA_Result addResult = cna_game_components_add(game, component);
    cna_game_components_get_count(game, &componentsAfter);
    CNA_Bool contains = CNA_FALSE, drawable = CNA_TRUE, enabled = CNA_FALSE;
    cna_game_components_contains(game, component, &contains);
    int32_t index = -1, order = 0;
    cna_game_components_index_of(game, component, &index);
    cna_game_component_get_is_drawable(component, &drawable);
    cna_game_component_get_enabled(component, &enabled);
    cna_game_component_set_update_order(component, 7);
    cna_game_component_get_update_order(component, &order);
    char componentType[96];
    uint64_t componentTypeBytes = 0;
    const CNA_Result componentTypeResult = cna_game_component_copy_type_name(component, componentType, sizeof componentType - 1, &componentTypeBytes);
    cprobe_terminate(componentType, sizeof componentType, componentTypeResult, &componentTypeBytes);
    g->component = component;
    cprobe_got(p, "create %u, components %llu -> add %u -> %llu, contains %u at %d, drawable %u, enabled %u, update order %d, \"%s\"", (unsigned)componentResult, (unsigned long long)components,
               (unsigned)addResult, (unsigned long long)componentsAfter, contains, (int)index, drawable, enabled, (int)order, componentType);
    CPROBE_CHECK(p, "cna_game_component_callbacks_init/create/get_is_drawable/get_enabled/set+get_update_order/copy_type_name, cna_game_components_get_count/add/contains/index_of",
                 "one more component, found at the last index; not drawable, enabled by default (XNA GameComponent), update order 7; \"Microsoft.Xna.Framework.GameComponent\"",
                 componentResult == CNA_RESULT_SUCCESS && addResult == CNA_RESULT_SUCCESS && componentsAfter == components + 1u && contains == CNA_TRUE && index == (int32_t)components &&
                     drawable == CNA_FALSE && enabled == CNA_TRUE && order == 7 && strcmp(componentType, "Microsoft.Xna.Framework.GameComponent") == 0);
}

void gp_after_game(GameProbe* g)
{
    CProbe* p = &g->probe;
    /* A new game may be created once the previous one has fully closed; the process then still has one game at most. */
    CNA_GameCreateInfo info;
    VERSIONED(info);
    info.target_elapsed_time_ticks = INT64_C(166667);
    CNA_Handle again = CNA_INVALID_HANDLE;
    const CNA_Result created = cna_game_create(&info, &again);
    const CNA_Result destroyed = created == CNA_RESULT_SUCCESS ? cna_game_destroy(again) : CNA_RESULT_INVALID_STATE;
    cprobe_got(p, "second game after the first closed: create %u, destroy %u", (unsigned)created, (unsigned)destroyed);
    CPROBE_CHECK(p, "cna_game_create after cna_game_destroy", "SUCCESS: a new game may be created only after the previous handle has fully closed, and it can be destroyed again",
                 created == CNA_RESULT_SUCCESS && destroyed == CNA_RESULT_SUCCESS);
}
