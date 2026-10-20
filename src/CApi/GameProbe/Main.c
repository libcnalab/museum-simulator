/* The C game probe's program: creates one CNA game through the C ABI, runs GAME_PROBE_FRAMES frames with a family of
 * checks in each update (and the drawing checks in a draw), destroys everything in the documented order and prints
 *     [c-probe] game: <name>: PASS|FAIL|SKIP (expected ..., got ...)
 * for every check, then a summary line. Usage: cna-c-probe <content root> [<isolated fixture home>]. It runs only as a
 * child of the museum (the cx-capi room) or under the museum's tools, on their private display. */
#include <stdarg.h>
#include <stdlib.h>

#include "CApi/GameProbe/GameProbe.h"

static int skips;

static void PrintCheck(void* user, const char* name, const char* expected, const char* got, int pass)
{
    (void)user;
    printf("[c-probe] game: %s: %s (expected %s, got %s)\n", name, pass ? "PASS" : "FAIL", expected, got);
    fflush(stdout);
}

void game_probe_skip(GameProbe* g, const char* name, const char* why)
{
    (void)g;
    skips += 1;
    printf("[c-probe] game: %s: SKIP (%s)\n", name, why);
    fflush(stdout);
}

static CNA_Result OnLoad(CNA_Handle game, const CNA_GameTime* time, void* context, CNA_CallbackError* error)
{
    (void)game;
    (void)error;
    GameProbe* g = (GameProbe*)context;
    g->loads += 1;
    g->loadFrame = time == NULL ? g->updates : -2; /* load runs before the first update and receives no game time */
    return CNA_RESULT_SUCCESS;
}

static CNA_Result OnUpdate(CNA_Handle game, const CNA_GameTime* time, void* context, CNA_CallbackError* error)
{
    GameProbe* g = (GameProbe*)context;
    g->updates += 1;
    if (time != NULL) {
        if (time->total_game_time_ticks < g->lastTotalTicks) g->lastTotalTicks = -1; /* time must never run backwards */
        else if (g->lastTotalTicks >= 0) g->lastTotalTicks = time->total_game_time_ticks;
    }
    CNA_Handle device = CNA_INVALID_HANDLE;
    if (cna_game_get_graphics_device(game, &device) != CNA_RESULT_SUCCESS) {
        static const char message[] = "the update callback could not borrow the graphics device";
        error->message.data = message;
        error->message.byte_length = sizeof message - 1;
        return CNA_RESULT_INVALID_STATE;
    }
    switch (g->updates) {
        case 1: gp_runtime(g, game, device); break;
        case 3: gp_graphics_more(g, game, device); break;
        case 4: gp_effects(g, game, device); break;
        case 5: gp_media(g, game, device); break;
        case 6: gp_input(g, game, device); break;
        case 7: gp_services(g, game, device); break;
        default: break;
    }
    /* The first update of cna_game_run (after the stepped frames) asks the loop to exit, as a game calling Exit() does. */
    if (g->updates == GAME_PROBE_FRAMES + 1) g->exitRequest = cna_game_request_exit(game);
    return CNA_RESULT_SUCCESS;
}

static CNA_Result OnDraw(CNA_Handle game, const CNA_GameTime* time, void* context, CNA_CallbackError* error)
{
    (void)time;
    (void)error;
    GameProbe* g = (GameProbe*)context;
    g->draws += 1;
    if (g->updates == 2) {
        CNA_Handle device = CNA_INVALID_HANDLE;
        if (cna_game_get_graphics_device(game, &device) == CNA_RESULT_SUCCESS) gp_graphics(g, game, device);
    }
    cna_game_clear(game, (CNA_Color){20, 24, 40, 255});
    return CNA_RESULT_SUCCESS;
}

static CNA_Result OnUnload(CNA_Handle game, const CNA_GameTime* time, void* context, CNA_CallbackError* error)
{
    (void)game;
    (void)time;
    (void)error;
    ((GameProbe*)context)->unloads += 1;
    return CNA_RESULT_SUCCESS;
}

static CNA_Result OnExiting(CNA_Handle game, const CNA_GameTime* time, void* context, CNA_CallbackError* error)
{
    (void)game;
    (void)time;
    (void)error;
    ((GameProbe*)context)->exits += 1;
    return CNA_RESULT_SUCCESS;
}

static void OnExitingEvent(void* context)
{
    ((GameProbe*)context)->exitingEvents += 1;
}

static void OnDisposedEvent(void* context)
{
    ((GameProbe*)context)->disposedEvents += 1;
}

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    static GameProbe probe;
    memset(&probe, 0, sizeof probe);
    probe.probe.report = PrintCheck;
    probe.loadFrame = -1;
    snprintf(probe.contentRoot, sizeof probe.contentRoot, "%s", argc > 1 ? argv[1] : "Content");
    snprintf(probe.fixtureHome, sizeof probe.fixtureHome, "%s", argc > 2 ? argv[2] : "");
    GameProbe* g = &probe;
    CProbe* p = &probe.probe;
    printf("[c-probe] start: CNA C ABI %u.%u.%u, content root %s\n", (unsigned)(cna_get_abi_version() >> 16), (unsigned)((cna_get_abi_version() >> 8) & 0xFFu),
           (unsigned)(cna_get_abi_version() & 0xFFu), probe.contentRoot);

    gp_before_game(g);

    CNA_GameCallbacks callbacks;
    VERSIONED(callbacks);
    callbacks.load_content = OnLoad;
    callbacks.update = OnUpdate;
    callbacks.draw = OnDraw;
    callbacks.unload_content = OnUnload;
    callbacks.exiting = OnExiting;
    callbacks.context = g;
    static const char title[] = "CNA C game probe";
    CNA_GameCreateInfo createInfo;
    VERSIONED(createInfo);
    createInfo.is_fixed_time_step = CNA_FALSE;
    createInfo.target_elapsed_time_ticks = INT64_C(166667);
    createInfo.window_title.data = title;
    createInfo.window_title.byte_length = sizeof title - 1;
    createInfo.callbacks = &callbacks;
    CNA_Handle game = CNA_INVALID_HANDLE, second = CNA_INVALID_HANDLE;
    const CNA_Result created = cna_game_create(&createInfo, &game);
    const CNA_Result secondResult = cna_game_create(&createInfo, &second);
    cprobe_got(p, "create %u (handle %s), a second game while it lives -> %u", (unsigned)created, game != CNA_INVALID_HANDLE ? "valid" : "invalid", (unsigned)secondResult);
    CPROBE_CHECK(p, "cna_game_create, and a second cna_game_create while the first lives", "SUCCESS; the second refused with CNA_RESULT_INVALID_STATE (one C-owned game per process)",
                 created == CNA_RESULT_SUCCESS && game != CNA_INVALID_HANDLE && secondResult == CNA_RESULT_INVALID_STATE);
    if (created != CNA_RESULT_SUCCESS) {
        char message[256];
        printf("[c-probe] done: the game could not be created (%s); %d checks, %d failed, %d skipped\n", cprobe_last_message(message, sizeof message), p->checks, p->failures, skips);
        return 2;
    }

    CNA_GameEventRegistrationHandle exiting = CNA_INVALID_HANDLE, disposed = CNA_INVALID_HANDLE;
    cna_game_subscribe(game, CNA_GAME_EVENT_EXITING, OnExitingEvent, g, &exiting);
    cna_game_subscribe(game, CNA_GAME_EVENT_DISPOSED, OnDisposedEvent, g, &disposed);
    {
        /* Before the run begins the owned handle may borrow the device that ApplyChanges created in the constructor. */
        CNA_Handle early = CNA_INVALID_HANDLE;
        const CNA_Result earlyResult = cna_game_get_graphics_device(game, &early);
        CNA_RendererInfo info;
        VERSIONED(info);
        const CNA_Result infoResult = cna_graphics_device_get_renderer_info(early, &info);
        cna_graphics_device_supports_capability(early, CNA_GRAPHICS_CAPABILITY_THREE_D, &probe.threeD);
        CNA_Bool latched = CNA_FALSE;
        CNA_GraphicsRendererType active = 0;
        cna_graphics_renderer_get_is_latched_ext(&latched);
        const CNA_Result activeResult = cna_graphics_renderer_get_active_ext(&active);
        const CNA_Result late = cna_graphics_renderer_set_preferred_ext(active);
        cprobe_got(p, "borrow before the run %u, renderer info %u (type %u, max texture %u), 3D %u; latched %u, active %u (%u); set_preferred afterwards -> %u", (unsigned)earlyResult,
                   (unsigned)infoResult, (unsigned)info.renderer_type, (unsigned)info.max_texture_dimension, probe.threeD, latched, (unsigned)active, (unsigned)activeResult, (unsigned)late);
        CPROBE_CHECK(p, "cna_game_get_graphics_device before the run, cna_graphics_device_get_renderer_info, get_is_latched/get_active/set_preferred_ext after the device exists",
                     "the constructor's device is borrowable; the selection is latched and the active renderer is the device's; a late set_preferred -> CNA_RESULT_INVALID_STATE",
                     earlyResult == CNA_RESULT_SUCCESS && infoResult == CNA_RESULT_SUCCESS && latched == CNA_TRUE && activeResult == CNA_RESULT_SUCCESS && active == info.renderer_type &&
                         late == CNA_RESULT_INVALID_STATE);
    }

    int frameFailures = 0;
    CNA_Result frameResult = CNA_RESULT_SUCCESS;
    for (int frame = 0; frame < GAME_PROBE_FRAMES; ++frame) {
        frameResult = cna_game_run_one_frame(game);
        if (frameResult != CNA_RESULT_SUCCESS) {
            frameFailures += 1;
            char message[256];
            printf("[c-probe] frame %d: cna_game_run_one_frame -> %u (%s)\n", frame, (unsigned)frameResult, cprobe_last_message(message, sizeof message));
            break;
        }
    }
    cprobe_got(p, "%d frames run (failures %d), loads %d at update %d, updates %d, draws %d, total game time monotonic %s", GAME_PROBE_FRAMES, frameFailures, probe.loads, probe.loadFrame,
               probe.updates, probe.draws, probe.lastTotalTicks >= 0 ? "yes" : "no");
    CPROBE_CHECK(p, "cna_game_run_one_frame x10 with C lifecycle callbacks", "every frame SUCCESS; load_content once, before the first update and without game time; one update and one draw per frame (variable step); time never runs backwards",
                 frameFailures == 0 && probe.loads == 1 && probe.loadFrame == 0 && probe.updates == GAME_PROBE_FRAMES && probe.draws == GAME_PROBE_FRAMES && probe.lastTotalTicks >= 0);

    /* Then the game's own loop: cna_game_run runs until the exit requested in its first update. */
    const int drawsBeforeRun = probe.draws;
    const CNA_Result ran = cna_game_run(game);
    cprobe_got(p, "run %u after %d update(s), %d draw(s) in the run; request_exit %u; exiting callback %d, EXITING event %d", (unsigned)ran, probe.updates - GAME_PROBE_FRAMES,
               probe.draws - drawsBeforeRun, (unsigned)probe.exitRequest, probe.exits, probe.exitingEvents);
    CPROBE_CHECK(p, "cna_game_run until cna_game_request_exit from an update callback",
                 "SUCCESS after one update and no draw (XNA's DrawFrame skips the frame once Exit was requested); the exiting callback and the EXITING event once each (HostExiting raises OnExiting)",
                 ran == CNA_RESULT_SUCCESS && probe.updates == GAME_PROBE_FRAMES + 1 && probe.draws == drawsBeforeRun && probe.exitRequest == CNA_RESULT_SUCCESS && probe.exits == 1 &&
                     probe.exitingEvents == 1);
    probe.exitsBeforeDestroy = probe.exits;
    probe.exitingEventsBeforeDestroy = probe.exitingEvents;
    /* The component the game has driven since frame 1. */
    if (probe.component != CNA_INVALID_HANDLE) {
        CNA_Bool removed = CNA_FALSE;
        const CNA_Result removeResult = cna_game_components_remove(game, probe.component, &removed);
        const CNA_Result componentDestroyed = cna_game_component_destroy(probe.component);
        cprobe_got(p, "initialized %d, updated %d times, remove %u (%u), destroy %u", probe.componentInitialized, probe.componentUpdates, (unsigned)removeResult, removed, (unsigned)componentDestroyed);
        CPROBE_CHECK(p, "the C game component during the run: initialize/update callbacks, cna_game_components_remove, cna_game_component_destroy",
                     "initialized once when added to the running game, updated once in every later update: the 9 stepped frames after frame 1 and the run's one (10, or 11 counting frame 1's own), then removed and destroyed",
                     probe.componentInitialized == 1 && (probe.componentUpdates == GAME_PROBE_FRAMES || probe.componentUpdates == GAME_PROBE_FRAMES + 1) && removeResult == CNA_RESULT_SUCCESS &&
                         removed == CNA_TRUE && componentDestroyed == CNA_RESULT_SUCCESS);
    }
    /* Children first, then the game (CALLBACKS_AND_THREADING.md, shutdown order). */
    if (probe.deviceManager != CNA_INVALID_HANDLE) cna_graphics_device_manager_destroy(probe.deviceManager);
    if (probe.spriteBatch != CNA_INVALID_HANDLE) cna_sprite_batch_destroy(probe.spriteBatch);
    if (probe.target != CNA_INVALID_HANDLE) cna_render_target_destroy(probe.target);
    const CNA_Result destroyed = cna_game_destroy(game);
    if (destroyed != CNA_RESULT_SUCCESS) {
        char message[300];
        printf("[c-probe] cna_game_destroy -> %u: %s\n", (unsigned)destroyed, cprobe_last_message(message, sizeof message));
    }
    CNA_Bool active = CNA_TRUE;
    const CNA_Result stale = cna_game_get_is_active(game, &active);
    const CNA_Result unsubscribed = cna_game_unsubscribe(exiting);
    const CNA_Result unsubscribedTwice = cna_game_unsubscribe(exiting);
    cna_game_unsubscribe(disposed);
    cprobe_got(p, "destroy %u; unload %d, exiting %d, EXITING event %d, DISPOSED event %d; the destroyed handle -> %u; unsubscribe afterwards %u, again %u", (unsigned)destroyed, probe.unloads,
               probe.exits, probe.exitingEvents, probe.disposedEvents, (unsigned)stale, (unsigned)unsubscribed, (unsigned)unsubscribedTwice);
    CPROBE_CHECK(p, "cna_game_destroy, cna_game_subscribe(DISPOSED)/unsubscribe, a destroyed game handle",
                 "SUCCESS: unload_content once, the exit not repeated, DISPOSED raised once; the dead handle -> CNA_RESULT_INVALID_HANDLE; unsubscribing afterwards succeeds once, then INVALID_HANDLE",
                 destroyed == CNA_RESULT_SUCCESS && probe.unloads == 1 && probe.exits == 1 && probe.exitingEvents == 1 && probe.disposedEvents == 1 && stale == CNA_RESULT_INVALID_HANDLE &&
                     unsubscribed == CNA_RESULT_SUCCESS && unsubscribedTwice == CNA_RESULT_INVALID_HANDLE);

    gp_after_game(g);
    printf("[c-probe] done: %d checks, %d failed, %d skipped\n", p->checks, p->failures, skips);
    return p->failures > 0 ? 1 : 0;
}
