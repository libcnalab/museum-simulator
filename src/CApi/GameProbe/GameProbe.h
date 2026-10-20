/* The museum's C game probe: a separate C17 program that creates its own CNA game through the C ABI (one game per
 * process, so it cannot live inside the museum), runs a fixed number of frames doing checks and prints one line per
 * check. The museum's cx-capi room starts it as a child process and logs every line. C only. */
#ifndef MUSEUM_C_GAME_PROBE_H
#define MUSEUM_C_GAME_PROBE_H

#include "CApi/CProbe.h"
#include "CNA/C/cna.h"

#define GAME_PROBE_FRAMES 10

#define VERSIONED(value)                              \
    do {                                              \
        memset(&(value), 0, sizeof(value));           \
        (value).struct_size = (uint32_t)sizeof(value); \
        (value).struct_version = 1u;                  \
    } while (0)

typedef struct GameProbe {
    CProbe probe;
    /* Lifecycle bookkeeping (Main.c). */
    int loads, updates, draws, unloads, exits;
    int exitingEvents, disposedEvents;
    CNA_Result exitRequest;
    int exitsBeforeDestroy, exitingEventsBeforeDestroy;
    int loadFrame;
    int64_t lastTotalTicks;
    /* Where the museum's content and the isolated fixture home are (argv). */
    char contentRoot[1024];
    char fixtureHome[1024];
    /* Whether the active renderer draws 3D (the drawing checks report SKIP when it does not). */
    CNA_Bool threeD;
    /* Owned resources that outlive one callback (Graphics.c). */
    CNA_Handle target;
    CNA_Handle spriteBatch;
    /* A game component added in frame 1 and counted until the end (Runtime.c). */
    CNA_Handle component;
    int componentInitialized, componentUpdates;
    CNA_Handle deviceManager;
} GameProbe;

/** Reports a check that could not run here (never counted as a pass). */
void game_probe_skip(GameProbe* g, const char* name, const char* why);

/* Families, each in its own file. "game" is the callback-borrowed handle, "device" the borrowed graphics device. */
void gp_before_game(GameProbe* g);                                 /* Runtime.c: renderer selection before the latch, storage */
void gp_runtime(GameProbe* g, CNA_Handle game, CNA_Handle device); /* Runtime.c: game, window, timing, launch parameters, device manager, components */
void gp_graphics(GameProbe* g, CNA_Handle game, CNA_Handle device);    /* Graphics.c: device, textures, render targets, states, buffers, drawing */
void gp_graphics_more(GameProbe* g, CNA_Handle game, CNA_Handle device); /* Graphics.c: volumes, display, resources, graphics_ext */
void gp_effects(GameProbe* g, CNA_Handle game, CNA_Handle device);     /* Effects.c: effects, models, sprite fonts, content */
void gp_media(GameProbe* g, CNA_Handle game, CNA_Handle device);       /* Media.c: audio, XACT, songs, media player, video, media library */
void gp_input(GameProbe* g, CNA_Handle game, CNA_Handle device);       /* Input.c: captured input, cursor, text input, haptics, joysticks, devices, sensors */
void gp_services(GameProbe* g, CNA_Handle game, CNA_Handle device);    /* Services.c: content readers over storage, gamer services, sessions */
void gp_after_game(GameProbe* g);                                  /* Runtime.c: what a destroyed game answers */

#endif
