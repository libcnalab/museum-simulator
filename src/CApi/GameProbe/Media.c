/* The C game probe's audio and media: PCM sound effects and their instances and XNA's sample arithmetic (audio.h),
 * the museum's XACT project (xact.h: engine, categories, global and cue variables, banks, cues), songs and the media
 * player (media.h, media_player.h), video metadata and a video player (video.h), and the media library indexed from
 * the isolated fixture home the museum gives this process (media_library.h). The library is read only when HOME is
 * that fixture copy: never the visitor's own music or pictures. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "CApi/GameProbe/GameProbe.h"

static void Audio(GameProbe* g, CNA_Handle game, CNA_Handle content)
{
    CProbe* p = &g->probe;
    /* XNA's static sample arithmetic: bytes <-> duration for 16-bit PCM. */
    int64_t halfSecond = 0;
    int32_t oneSecondMono = 0;
    cna_sound_effect_get_sample_duration_ticks(88200, 44100, CNA_AUDIO_CHANNELS_STEREO, &halfSecond);
    cna_sound_effect_get_sample_size_in_bytes(INT64_C(10000000), 22050, CNA_AUDIO_CHANNELS_MONO, &oneSecondMono);
    int64_t ignored = 0;
    const CNA_Result badChannels = cna_sound_effect_get_sample_duration_ticks(100, 44100, 3u, &ignored);
    CNA_AudioEmitter emitter;
    CNA_AudioListener listener;
    VERSIONED(emitter);
    VERSIONED(listener);
    cna_audio_emitter_init(&emitter);
    cna_audio_listener_init(&listener);
    cprobe_got(p, "88200 bytes at 44100 Hz stereo = %lld ticks, 1 s at 22050 Hz mono = %d bytes, 3 channels -> %u; emitter forward (%g, %g, %g) doppler %g, listener up (%g, %g, %g)",
               (long long)halfSecond, (int)oneSecondMono, (unsigned)badChannels, (double)emitter.forward.x, (double)emitter.forward.y, (double)emitter.forward.z, (double)emitter.doppler_scale,
               (double)listener.up.x, (double)listener.up.y, (double)listener.up.z);
    CPROBE_CHECK(p, "cna_sound_effect_get_sample_duration_ticks/get_sample_size_in_bytes, cna_audio_emitter_init, cna_audio_listener_init",
                 "0.5 s = 5000000 ticks; 1 s = 44098 bytes, XNA's AudioFormat.SizeFromDuration ((int)(1000 * 22.05f) = 22049 whole frames, the float rate rounds down); an undefined channel count refused; XNA defaults: forward (0, 0, -1), DopplerScale 1, up (0, 1, 0)",
                 halfSecond == INT64_C(5000000) && oneSecondMono == 44098 && badChannels == CNA_RESULT_INVALID_ARGUMENT && emitter.forward.z == -1.0f && emitter.doppler_scale == 1.0f &&
                     listener.up.y == 1.0f);

    CNA_AudioCapabilities capabilities;
    VERSIONED(capabilities);
    cna_audio_get_capabilities(game, &capabilities);
    if (capabilities.is_playback_available != CNA_TRUE) {
        game_probe_skip(g, "PCM sound effect, instance, content-loaded sound effect and XACT", "no audio playback device in this environment");
        return;
    }
    /* 2205 mono frames at 22050 Hz: exactly 0.1 s. */
    static uint8_t pcm[2205 * 2];
    for (int i = 0; i < 2205; ++i) {
        const int16_t sample = (int16_t)(8000.0 * sin(2.0 * 3.14159265358979 * 440.0 * i / 22050.0));
        pcm[i * 2] = (uint8_t)(sample & 0xFF);
        pcm[i * 2 + 1] = (uint8_t)((sample >> 8) & 0xFF);
    }
    CNA_SoundEffectCreateInfo create;
    VERSIONED(create);
    create.sample_rate = 22050u;
    create.channels = CNA_AUDIO_CHANNELS_MONO;
    CNA_Handle effect = CNA_INVALID_HANDLE;
    const CNA_Result created = cna_sound_effect_create_pcm16(game, &create, pcm, sizeof pcm, &effect);
    int64_t duration = 0;
    cna_sound_effect_get_duration_ticks(effect, &duration);
    CNA_Handle oddFrames = CNA_INVALID_HANDLE;
    const CNA_Result odd = cna_sound_effect_create_pcm16(game, &create, pcm, 3u, &oddFrames);
    CNA_Handle instance = CNA_INVALID_HANDLE;
    cna_sound_effect_create_instance(effect, &instance);
    CNA_SoundEffectInstanceInfo before, playing, stopped;
    VERSIONED(before);
    VERSIONED(playing);
    VERSIONED(stopped);
    cna_sound_effect_instance_get_info(instance, &before);
    cna_sound_effect_instance_set_volume(instance, 0.5f);
    cna_sound_effect_instance_set_pan(instance, -0.25f);
    cna_sound_effect_instance_set_is_looped(instance, CNA_TRUE);
    cna_sound_effect_instance_play(instance);
    cna_sound_effect_instance_get_info(instance, &playing);
    cna_sound_effect_instance_stop(instance, CNA_TRUE);
    cna_sound_effect_instance_get_info(instance, &stopped);
    const CNA_Result effectWhileInstance = cna_sound_effect_destroy(effect);
    cna_sound_effect_instance_destroy(instance);
    const CNA_Result effectDestroyed = cna_sound_effect_destroy(effect);
    float master = 0.0f;
    cna_sound_effect_get_master_volume(game, &master);
    cprobe_got(p, "create %u, duration %lld ticks, 3 bytes -> %u; instance before: state %u volume %g pitch %g pan %g looped %u; playing: state %u volume %g pan %g looped %u; stopped: state %u; effect destroy with a live instance %u, after %u; master volume %g",
               (unsigned)created, (long long)duration, (unsigned)odd, (unsigned)before.state, (double)before.volume, (double)before.pitch, (double)before.pan, before.is_looped,
               (unsigned)playing.state, (double)playing.volume, (double)playing.pan, playing.is_looped, (unsigned)stopped.state, (unsigned)effectWhileInstance, (unsigned)effectDestroyed,
               (double)master);
    CPROBE_CHECK(p, "cna_audio_get_capabilities, cna_sound_effect_create_pcm16/get_duration_ticks/create_instance/destroy/get_master_volume, cna_sound_effect_instance_get_info/set_volume/set_pan/set_is_looped/play/stop/destroy",
                 "0.1 s = 1000000 ticks; a partial frame refused; a new instance Stopped with volume 1, pitch 0, pan 0; Playing with the set values; Stopped again; the effect outlives its instances only (INVALID_STATE first); master volume 1",
                 created == CNA_RESULT_SUCCESS && duration == INT64_C(1000000) && odd == CNA_RESULT_INVALID_ARGUMENT && before.state == CNA_SOUND_STATE_STOPPED && before.volume == 1.0f &&
                     before.pitch == 0.0f && before.pan == 0.0f && playing.state == CNA_SOUND_STATE_PLAYING && playing.volume == 0.5f && playing.pan == -0.25f && playing.is_looped == CNA_TRUE &&
                     stopped.state == CNA_SOUND_STATE_STOPPED && effectWhileInstance == CNA_RESULT_INVALID_STATE && effectDestroyed == CNA_RESULT_SUCCESS && master == 1.0f);

    /* An XNA-compiled SoundEffect .xnb through the content manager. */
    CNA_Handle loaded = CNA_INVALID_HANDLE;
    const CNA_Result loadedResult = cna_content_manager_load_sound_effect(content, cprobe_view("Compiled/xnb/sound"), &loaded);
    int64_t loadedDuration = 0;
    cna_sound_effect_get_duration_ticks(loaded, &loadedDuration);
    char loadedType[96];
    uint64_t loadedTypeBytes = 0;
    const CNA_Result loadedTyped = cna_sound_effect_copy_type_name(loaded, loadedType, sizeof loadedType - 1, &loadedTypeBytes);
    cprobe_terminate(loadedType, sizeof loadedType, loadedTyped, &loadedTypeBytes);
    if (loaded != CNA_INVALID_HANDLE) cna_sound_effect_destroy(loaded);
    cprobe_got(p, "load %u, duration %lld ticks, \"%s\"", (unsigned)loadedResult, (long long)loadedDuration, loadedType);
    CPROBE_CHECK(p, "cna_content_manager_load_sound_effect (the museum's XNA-compiled sound.xnb), cna_sound_effect_copy_type_name", "a sound with a positive duration; \"Microsoft.Xna.Framework.Audio.SoundEffect\"",
                 loadedResult == CNA_RESULT_SUCCESS && loadedDuration > 0 && strcmp(loadedType, "Microsoft.Xna.Framework.Audio.SoundEffect") == 0);

    /* The museum's XACT project (tools/gen_xact.py): 3 categories, MasterTrim (global, 0..1, initially 1), Distance (per cue, 0..100). */
    char path[1200];
    snprintf(path, sizeof path, "%s/Xact/museum.xgs", g->contentRoot);
    CNA_Handle engine = CNA_INVALID_HANDLE, missingEngine = CNA_INVALID_HANDLE;
    const CNA_Result missing = cna_audio_engine_create(game, cprobe_view("no-such.xgs"), &missingEngine);
    const CNA_Result engineCreated = cna_audio_engine_create(game, cprobe_view(path), &engine);
    float trim = -1.0f, trimHalf = -1.0f, trimClamped = -1.0f;
    cna_audio_engine_get_global_variable(engine, cprobe_view("MasterTrim"), &trim);
    cna_audio_engine_set_global_variable(engine, cprobe_view("MasterTrim"), 0.5f);
    cna_audio_engine_get_global_variable(engine, cprobe_view("MasterTrim"), &trimHalf);
    cna_audio_engine_set_global_variable(engine, cprobe_view("MasterTrim"), 7.0f);
    cna_audio_engine_get_global_variable(engine, cprobe_view("MasterTrim"), &trimClamped);
    CNA_Handle category = CNA_INVALID_HANDLE;
    cna_audio_engine_get_category(engine, cprobe_view("Music"), &category);
    char categoryName[32];
    uint64_t categoryBytes = 0;
    const CNA_Result categoryNamed = cna_audio_category_copy_name(category, categoryName, sizeof categoryName - 1, &categoryBytes);
    cprobe_terminate(categoryName, sizeof categoryName, categoryNamed, &categoryBytes);
    if (category != CNA_INVALID_HANDLE) cna_audio_category_destroy(category);
    snprintf(path, sizeof path, "%s/Xact/museum.xwb", g->contentRoot);
    CNA_Handle waves = CNA_INVALID_HANDLE, sounds = CNA_INVALID_HANDLE, cue = CNA_INVALID_HANDLE;
    const CNA_Result wavesCreated = cna_wave_bank_create(engine, cprobe_view(path), &waves);
    snprintf(path, sizeof path, "%s/Xact/museum.xsb", g->contentRoot);
    const CNA_Result soundsCreated = cna_sound_bank_create(engine, cprobe_view(path), &sounds);
    const CNA_Result cueCreated = cna_sound_bank_get_cue(sounds, cprobe_view("Tone"), &cue);
    char cueName[32];
    uint64_t cueBytes = 0;
    const CNA_Result cueNamed = cna_cue_copy_name(cue, cueName, sizeof cueName - 1, &cueBytes);
    cprobe_terminate(cueName, sizeof cueName, cueNamed, &cueBytes);
    float distance = -1.0f, distanceSet = -1.0f;
    cna_cue_get_variable(cue, cprobe_view("Distance"), &distance);
    cna_cue_set_variable(cue, cprobe_view("Distance"), 40.0f);
    cna_cue_get_variable(cue, cprobe_view("Distance"), &distanceSet);
    CNA_CueInfo prepared, played;
    VERSIONED(prepared);
    VERSIONED(played);
    cna_cue_get_info(cue, &prepared);
    cna_cue_play(cue);
    cna_cue_get_info(cue, &played);
    cna_cue_stop(cue, CNA_AUDIO_STOP_OPTIONS_IMMEDIATE);
    if (cue != CNA_INVALID_HANDLE) cna_cue_destroy(cue);
    if (sounds != CNA_INVALID_HANDLE) cna_sound_bank_destroy(sounds);
    if (waves != CNA_INVALID_HANDLE) cna_wave_bank_destroy(waves);
    if (engine != CNA_INVALID_HANDLE) cna_audio_engine_destroy(engine);
    cprobe_got(p, "missing .xgs -> %u; engine %u: MasterTrim %g -> %g -> (7) %g; category \"%s\"; wave bank %u, sound bank %u, cue %u \"%s\", Distance %g -> %g, prepared %u, playing after Play %u",
               (unsigned)missing, (unsigned)engineCreated, (double)trim, (double)trimHalf, (double)trimClamped, categoryName, (unsigned)wavesCreated, (unsigned)soundsCreated, (unsigned)cueCreated,
               cueName, (double)distance, (double)distanceSet, prepared.is_prepared, played.is_playing);
    CPROBE_CHECK(p, "cna_audio_engine_create/get_global_variable/set_global_variable/get_category/destroy, cna_audio_category_copy_name/destroy, cna_wave_bank_create/destroy, cna_sound_bank_create/get_cue/destroy, cna_cue_copy_name/get_variable/set_variable/get_info/play/stop/destroy",
                 "a missing settings file -> CNA_RESULT_IO; MasterTrim 1, then 0.5, then clamped to its maximum 1; the \"Music\" category; the banks load; cue \"Tone\" with Distance 0 then 40, prepared, playing after Play",
                 missing == CNA_RESULT_IO && engineCreated == CNA_RESULT_SUCCESS && trim == 1.0f && trimHalf == 0.5f && trimClamped == 1.0f && strcmp(categoryName, "Music") == 0 &&
                     wavesCreated == CNA_RESULT_SUCCESS && soundsCreated == CNA_RESULT_SUCCESS && cueCreated == CNA_RESULT_SUCCESS && strcmp(cueName, "Tone") == 0 && distance == 0.0f &&
                     distanceSet == 40.0f && prepared.is_prepared == CNA_TRUE && played.is_playing == CNA_TRUE);
}

static void SongsAndVideo(GameProbe* g, CNA_Handle game, CNA_Handle device)
{
    CProbe* p = &g->probe;
    char path[1200];
    snprintf(path, sizeof path, "%s/Audio/arpeggio.mp3", g->contentRoot);
    CNA_SongHandle song = CNA_INVALID_HANDLE, twin = CNA_INVALID_HANDLE, missingSong = CNA_INVALID_HANDLE;
    const CNA_Result created = cna_song_create_with_duration(game, cprobe_view(path), cprobe_view("Arpeggio"), 1500, &song);
    cna_song_create(game, cprobe_view(path), cprobe_view("Same file"), &twin);
    const CNA_Result missing = cna_song_create(game, cprobe_view("no-such-song.mp3"), cprobe_view("x"), &missingSong);
    int64_t duration = 0;
    cna_song_get_duration(song, &duration);
    char name[32];
    uint64_t nameBytes = 0;
    const CNA_Result named = cna_song_copy_name(song, name, sizeof name - 1, &nameBytes);
    cprobe_terminate(name, sizeof name, named, &nameBytes);
    CNA_Bool equal = CNA_FALSE;
    cna_song_equals(song, twin, &equal);
    CNA_Bool control = CNA_FALSE;
    cna_media_player_get_game_has_control(game, &control);
    float volume = -1.0f, clamped = -1.0f;
    cna_media_player_set_volume(game, 0.25f);
    cna_media_player_get_volume(game, &volume);
    cna_media_player_set_volume(game, 1.5f);
    cna_media_player_get_volume(game, &clamped);
    CNA_MediaState initial = 99;
    cna_media_player_get_state(game, &initial);
    cna_media_player_set_is_repeating(game, CNA_TRUE);
    CNA_Bool repeating = CNA_FALSE;
    cna_media_player_get_is_repeating(game, &repeating);
    cna_media_player_set_is_repeating(game, CNA_FALSE);
    cprobe_got(p, "create %u: duration %lld ticks, name \"%s\", equals the other song over the file %u; a missing file -> %u; game has control %u; volume 0.25 -> %g, 1.5 -> %g; state %u; repeating %u",
               (unsigned)created, (long long)duration, name, equal, (unsigned)missing, control, (double)volume, (double)clamped, (unsigned)initial, repeating);
    CPROBE_CHECK(p, "cna_song_create/create_with_duration/get_duration/copy_name/equals, cna_media_player_get_game_has_control/set+get_volume/get_state/set+get_is_repeating",
                 "1500 ms = 15000000 ticks, \"Arpeggio\", equal by file path; a missing file -> CNA_RESULT_IO; the game has control; 0.25, then clamped to 1; Stopped; repeating read back",
                 created == CNA_RESULT_SUCCESS && duration == INT64_C(15000000) && strcmp(name, "Arpeggio") == 0 && equal == CNA_TRUE && missing == CNA_RESULT_IO && control == CNA_TRUE &&
                     volume == 0.25f && clamped == 1.0f && initial == CNA_MEDIA_STATE_STOPPED && repeating == CNA_TRUE);
    CNA_AudioCapabilities playback;
    VERSIONED(playback);
    cna_audio_get_capabilities(game, &playback);
    if (playback.is_playback_available == CNA_TRUE) {
        const CNA_Result played = cna_media_player_play_song(game, song);
        CNA_MediaState playing = 99, stopped = 99;
        cna_media_player_get_state(game, &playing);
        CNA_MediaQueueHandle queue = CNA_INVALID_HANDLE;
        cna_media_player_get_queue(game, &queue);
        int32_t queued = 0;
        cna_media_queue_get_count(queue, &queued);
        if (queue != CNA_INVALID_HANDLE) cna_media_queue_destroy(queue);
        cna_media_player_stop(game);
        cna_media_player_get_state(game, &stopped);
        cprobe_got(p, "play %u: state %u, queue %d; after stop %u", (unsigned)played, (unsigned)playing, (int)queued, (unsigned)stopped);
        CPROBE_CHECK(p, "cna_media_player_play_song/get_queue/stop, cna_media_queue_get_count/destroy", "Playing with the one song queued, Stopped after Stop",
                     played == CNA_RESULT_SUCCESS && playing == CNA_MEDIA_STATE_PLAYING && queued == 1 && stopped == CNA_MEDIA_STATE_STOPPED);
    } else {
        game_probe_skip(g, "cna_media_player_play_song", "no audio playback device in this environment");
    }
    cna_song_destroy(twin);
    cna_song_destroy(song);

    /* Video: XNA's metadata constructor, and a player that has played nothing. */
    snprintf(path, sizeof path, "%s/Video/zoom.webm", g->contentRoot);
    CNA_VideoHandle video = CNA_INVALID_HANDLE;
    const CNA_Result videoCreated = cna_video_create_with_metadata(device, cprobe_view(path), 2000, 320, 240, 30.0f, CNA_VIDEO_SOUNDTRACK_TYPE_MUSIC, &video);
    int32_t width = 0, height = 0;
    float fps = 0.0f;
    int64_t videoDuration = 0;
    cna_video_get_width(video, &width);
    cna_video_get_height(video, &height);
    cna_video_get_frames_per_second(video, &fps);
    cna_video_get_duration(video, &videoDuration);
    char videoType[96];
    uint64_t videoTypeBytes = 0;
    const CNA_Result videoTyped = cna_video_copy_type_name(video, videoType, sizeof videoType - 1, &videoTypeBytes);
    cprobe_terminate(videoType, sizeof videoType, videoTyped, &videoTypeBytes);
    CNA_VideoPlayerHandle player = CNA_INVALID_HANDLE;
    const CNA_Result playerCreated = cna_video_player_create(game, &player);
    CNA_MediaState playerState = 99;
    cna_video_player_get_state(player, &playerState);
    CNA_VideoHandle playerVideo = CNA_INVALID_HANDLE;
    CNA_Bool hasVideo = CNA_TRUE;
    cna_video_player_get_video(player, &playerVideo, &hasVideo);
    float playerVolume = 0.0f;
    cna_video_player_get_volume(player, &playerVolume);
    char playerType[96];
    uint64_t playerTypeBytes = 0;
    const CNA_Result playerTyped = cna_video_player_copy_type_name(player, playerType, sizeof playerType - 1, &playerTypeBytes);
    cprobe_terminate(playerType, sizeof playerType, playerTyped, &playerTypeBytes);
    if (player != CNA_INVALID_HANDLE) cna_video_player_destroy(player);
    if (video != CNA_INVALID_HANDLE) cna_video_destroy(video);
    CNA_VideoInfo info;
    VERSIONED(info);
    info.width = 7;
    cna_video_info_init(&info);
    cprobe_got(p, "video %u: %dx%d at %g fps, %lld ticks, \"%s\"; player %u: state %u, video %u, volume %g, \"%s\"; default info width %d", (unsigned)videoCreated, (int)width, (int)height,
               (double)fps, (long long)videoDuration, videoType, (unsigned)playerCreated, (unsigned)playerState, hasVideo, (double)playerVolume, playerType, (int)info.width);
    CPROBE_CHECK(p, "cna_video_create_with_metadata/get_width/get_height/get_frames_per_second/get_duration/copy_type_name/destroy, cna_video_player_create/get_state/get_video/get_volume/copy_type_name/destroy, cna_video_info_init",
                 "the declared 320x240 at 30 fps for 2000 ms = 20000000 ticks, \"Microsoft.Xna.Framework.Media.Video\"; a new player Stopped with no video and volume 1, \"Microsoft.Xna.Framework.Media.VideoPlayer\"; zeroed info",
                 videoCreated == CNA_RESULT_SUCCESS && width == 320 && height == 240 && fps == 30.0f && videoDuration == INT64_C(20000000) &&
                     strcmp(videoType, "Microsoft.Xna.Framework.Media.Video") == 0 && playerCreated == CNA_RESULT_SUCCESS && playerState == CNA_MEDIA_STATE_STOPPED && hasVideo == CNA_FALSE &&
                     playerVolume == 1.0f && strcmp(playerType, "Microsoft.Xna.Framework.Media.VideoPlayer") == 0 && info.width == 0);
}

static void Library(GameProbe* g, CNA_Handle game)
{
    CProbe* p = &g->probe;
    const char* home = getenv("HOME");
    if (g->fixtureHome[0] == '\0' || home == NULL || strcmp(home, g->fixtureHome) != 0) {
        game_probe_skip(g, "cna_media_library_* over the fixture", "HOME is not the isolated fixture copy; the visitor's own library is never read");
        return;
    }
    CNA_MediaLibraryHandle library = CNA_INVALID_HANDLE;
    const CNA_Result created = cna_media_library_create(game, &library);
    CNA_SongCollectionHandle songs = CNA_INVALID_HANDLE;
    CNA_AlbumCollectionHandle albums = CNA_INVALID_HANDLE;
    CNA_ArtistCollectionHandle artists = CNA_INVALID_HANDLE;
    CNA_GenreCollectionHandle genres = CNA_INVALID_HANDLE;
    CNA_PlaylistCollectionHandle playlists = CNA_INVALID_HANDLE;
    CNA_PictureCollectionHandle pictures = CNA_INVALID_HANDLE;
    cna_media_library_get_songs(library, &songs);
    cna_media_library_get_albums(library, &albums);
    cna_media_library_get_artists(library, &artists);
    cna_media_library_get_genres(library, &genres);
    cna_media_library_get_playlists(library, &playlists);
    cna_media_library_get_pictures(library, &pictures);
    int32_t songCount = -1, albumCount = -1, artistCount = -1, genreCount = -1, playlistCount = -1, pictureCount = -1;
    cna_song_collection_get_count(songs, &songCount);
    cna_album_collection_get_count(albums, &albumCount);
    cna_artist_collection_get_count(artists, &artistCount);
    cna_genre_collection_get_count(genres, &genreCount);
    cna_playlist_collection_get_count(playlists, &playlistCount);
    cna_picture_collection_get_count(pictures, &pictureCount);
    char typeName[96];
    uint64_t typeBytes = 0;
    const CNA_Result typed = cna_media_library_copy_type_name(library, typeName, sizeof typeName - 1, &typeBytes);
    cprobe_terminate(typeName, sizeof typeName, typed, &typeBytes);
    CNA_Bool disposedBefore = CNA_TRUE, disposedAfter = CNA_FALSE;
    cna_media_library_get_is_disposed(library, &disposedBefore);
    cna_media_library_dispose(library);
    cna_media_library_get_is_disposed(library, &disposedAfter);
    if (pictures != CNA_INVALID_HANDLE) cna_picture_collection_destroy(pictures);
    if (playlists != CNA_INVALID_HANDLE) cna_playlist_collection_destroy(playlists);
    if (genres != CNA_INVALID_HANDLE) cna_genre_collection_destroy(genres);
    if (artists != CNA_INVALID_HANDLE) cna_artist_collection_destroy(artists);
    if (albums != CNA_INVALID_HANDLE) cna_album_collection_destroy(albums);
    if (songs != CNA_INVALID_HANDLE) cna_song_collection_destroy(songs);
    if (library != CNA_INVALID_HANDLE) cna_media_library_destroy(library);
    cprobe_got(p, "create %u: %d songs, %d albums, %d artists, %d genres, %d playlist(s), %d pictures; \"%s\"; disposed %u -> %u", (unsigned)created, (int)songCount, (int)albumCount,
               (int)artistCount, (int)genreCount, (int)playlistCount, (int)pictureCount, typeName, disposedBefore, disposedAfter);
    CPROBE_CHECK(p, "cna_media_library_create/get_songs/get_albums/get_artists/get_genres/get_playlists/get_pictures/copy_type_name/get_is_disposed/dispose/destroy and the collections' get_count/destroy",
                 "exactly the fixture (Content/MediaLibrary): 3 songs, 2 albums, 2 artists, 2 genres, 1 playlist, 4 pictures; \"Microsoft.Xna.Framework.Media.MediaLibrary\"; Dispose sets IsDisposed",
                 created == CNA_RESULT_SUCCESS && songCount == 3 && albumCount == 2 && artistCount == 2 && genreCount == 2 && playlistCount == 1 && pictureCount == 4 &&
                     strcmp(typeName, "Microsoft.Xna.Framework.Media.MediaLibrary") == 0 && disposedBefore == CNA_FALSE && disposedAfter == CNA_TRUE);
}

void gp_media(GameProbe* g, CNA_Handle game, CNA_Handle device)
{
    CNA_ContentManagerCreateInfo create;
    VERSIONED(create);
    create.root_directory = cprobe_view(g->contentRoot);
    CNA_Handle content = CNA_INVALID_HANDLE;
    cna_content_manager_create(device, &create, &content);
    Audio(g, game, content);
    SongsAndVideo(g, game, device);
    Library(g, game);
    cna_content_manager_destroy(content);
}
