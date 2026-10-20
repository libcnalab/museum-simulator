/* The C game probe's services: a storage device, container, files and streams (storage.h) in the isolated data folder
 * the museum gives this process, an .xnb object graph written into a storage file and read back with a content reader
 * (content_readers.h), the offline GamerServices runtime (gamer_services.h: dispatcher, trial mode, the Guide's
 * keyboard, signed-in gamers) and a local network session with one signed-in gamer (net_sessions.h). No account,
 * no peer and no network traffic is involved. */
#include <stdio.h>

#include "CApi/GameProbe/GameProbe.h"

static int keyboardCallbacks;

static void OnKeyboard(void* context)
{
    (void)context;
    keyboardCallbacks += 1;
}

static void Put7Bit(uint8_t* out, size_t* at, uint32_t value)
{
    while (value >= 0x80u) {
        out[(*at)++] = (uint8_t)(value | 0x80u);
        value >>= 7;
    }
    out[(*at)++] = (uint8_t)value;
}

static void PutBytes(uint8_t* out, size_t* at, const void* data, size_t count)
{
    memcpy(out + *at, data, count);
    *at += count;
}

static void StorageAndReaders(GameProbe* g, CNA_Handle game)
{
    (void)game;
    CProbe* p = &g->probe;
    CNA_StorageDeviceHandle device = CNA_INVALID_HANDLE;
    const CNA_Result selected = cna_storage_device_show_selector(NULL, NULL, &device);
    CNA_Bool connected = CNA_FALSE;
    cna_storage_device_get_is_connected(device, &connected);
    int64_t freeSpace = 0, totalSpace = 0;
    cna_storage_device_get_free_space(device, &freeSpace);
    cna_storage_device_get_total_space(device, &totalSpace);
    CNA_StorageContainerHandle container = CNA_INVALID_HANDLE;
    const CNA_Result opened = cna_storage_container_open(device, cprobe_view("cx-capi C probe"), NULL, NULL, &container);
    char display[64], typeName[96];
    uint64_t displayBytes = 0, typeBytes = 0;
    const CNA_Result displayed = cna_storage_container_copy_display_name(container, display, sizeof display - 1, &displayBytes);
    cprobe_terminate(display, sizeof display, displayed, &displayBytes);
    const CNA_Result typed = cna_storage_container_copy_type_name(container, typeName, sizeof typeName - 1, &typeBytes);
    cprobe_terminate(typeName, sizeof typeName, typed, &typeBytes);
    cna_storage_container_create_directory(container, cprobe_view("saves"));
    CNA_Bool directory = CNA_FALSE;
    cna_storage_container_directory_exists(container, cprobe_view("saves"), &directory);
    const CNA_Result busyDevice = cna_storage_device_destroy(device);

    /* An .xnb object graph after its header: one type reader (XNA's Int32Reader), no shared resources, the root Int32 1234, then raw fields. */
    static uint8_t body[256];
    size_t length = 0;
    static const char readerName[] = "Microsoft.Xna.Framework.Content.Int32Reader";
    const int32_t zero = 0, value = 1234;
    const float floats[3] = {1.0f, 2.0f, 3.0f};
    const uint8_t color[4] = {10, 20, 30, 40};
    Put7Bit(body, &length, 1u);
    Put7Bit(body, &length, (uint32_t)(sizeof readerName - 1));
    PutBytes(body, &length, readerName, sizeof readerName - 1);
    PutBytes(body, &length, &zero, 4);
    Put7Bit(body, &length, 0u);
    Put7Bit(body, &length, 1u);
    PutBytes(body, &length, &value, 4);
    PutBytes(body, &length, floats, sizeof floats);
    PutBytes(body, &length, color, sizeof color);
    CNA_StorageStreamHandle writer = CNA_INVALID_HANDLE;
    const CNA_Result fileCreated = cna_storage_container_create_file(container, cprobe_view("graph.xnb"), &writer);
    const CNA_Result written = cna_storage_stream_write(writer, body, length);
    int64_t writtenLength = 0;
    cna_storage_stream_get_length(writer, &writtenLength);
    CNA_Bool canWrite = CNA_FALSE;
    cna_storage_stream_get_can_write(writer, &canWrite);
    const CNA_Result busyContainer = cna_storage_container_destroy(container);
    cna_storage_stream_flush(writer);
    cna_storage_stream_close(writer);
    CNA_StorageStreamHandle reading = CNA_INVALID_HANDLE;
    const CNA_Result openedFile = cna_storage_container_open_file_access(container, cprobe_view("graph.xnb"), CNA_FILE_MODE_OPEN, CNA_FILE_ACCESS_READ, &reading);
    CNA_Bool readOnlyCanWrite = CNA_TRUE;
    cna_storage_stream_get_can_write(reading, &readOnlyCanWrite);
    const uint8_t one = 1;
    const CNA_Result writeRefused = cna_storage_stream_write(reading, &one, 1u);
    uint8_t first[4] = {0, 0, 0, 0};
    uint64_t firstRead = 0;
    cna_storage_stream_read(reading, first, 1u, &firstRead);
    int64_t position = -1;
    cna_storage_stream_seek(reading, 0, CNA_SEEK_ORIGIN_BEGIN, &position);
    cprobe_got(p, "selector %u: connected %u, free %lld of %lld bytes; container %u \"%s\" (%s), directory saves %u; device destroy with a container open %u; file %u written %u (%zu bytes, length %lld, can write %u); container destroy with a stream open %u; reopened read-only %u: can write %u, write -> %u, first byte 0x%02X, seek to 0 -> %lld",
               (unsigned)selected, connected, (long long)freeSpace, (long long)totalSpace, (unsigned)opened, display, typeName, directory, (unsigned)busyDevice, (unsigned)fileCreated,
               (unsigned)written, length, (long long)writtenLength, canWrite, (unsigned)busyContainer, (unsigned)openedFile, readOnlyCanWrite, (unsigned)writeRefused, first[0], (long long)position);
    CPROBE_CHECK(p, "cna_storage_device_show_selector/get_is_connected/get_free_space/get_total_space/destroy, cna_storage_container_open/copy_display_name/copy_type_name/create_directory/directory_exists/create_file/open_file_access/destroy, cna_storage_stream_write/get_length/get_can_write/flush/close/read/seek",
                 "a connected device with 0 < free <= total; container \"cx-capi C probe\" of type Microsoft.Xna.Framework.Storage.StorageContainer; parents refuse destruction while a child is open (INVALID_STATE); the written file reopened read-only refuses a write (NOT_SUPPORTED) and starts with the reader count 0x01",
                 selected == CNA_RESULT_SUCCESS && connected == CNA_TRUE && freeSpace > 0 && freeSpace <= totalSpace && opened == CNA_RESULT_SUCCESS && strcmp(display, "cx-capi C probe") == 0 &&
                     strcmp(typeName, "Microsoft.Xna.Framework.Storage.StorageContainer") == 0 && directory == CNA_TRUE && busyDevice == CNA_RESULT_INVALID_STATE && fileCreated == CNA_RESULT_SUCCESS &&
                     written == CNA_RESULT_SUCCESS && writtenLength == (int64_t)length && canWrite == CNA_TRUE && busyContainer == CNA_RESULT_INVALID_STATE && openedFile == CNA_RESULT_SUCCESS &&
                     readOnlyCanWrite == CNA_FALSE && writeRefused == CNA_RESULT_NOT_SUPPORTED && firstRead == 1u && first[0] == 0x01 && position == 0);

    /* The content reader over that stream: type-reader table, the root object through dispatch, then the raw fields. */
    CNA_ContentReaderCreateInfo create;
    VERSIONED(create);
    create.content_manager = CNA_INVALID_HANDLE;
    create.stream = reading;
    create.asset_name = cprobe_view("graph");
    create.version = 5;
    create.platform = (uint8_t)'w';
    CNA_ContentReaderHandle reader = CNA_INVALID_HANDLE;
    const CNA_Result readerCreated = cna_content_reader_create(&create, &reader);
    char assetName[32];
    uint64_t assetBytes = 0;
    const CNA_Result named = cna_content_reader_copy_asset_name(reader, assetName, sizeof assetName - 1, &assetBytes);
    cprobe_terminate(assetName, sizeof assetName, named, &assetBytes);
    int32_t version = 0;
    uint8_t platform = 0;
    cna_content_reader_get_version(reader, &version);
    cna_content_reader_get_platform(reader, &platform);
    CNA_Handle manager = (CNA_Handle)1;
    cna_content_reader_get_content_manager(reader, &manager);
    const CNA_Result table = cna_content_reader_initialize_type_readers(reader);
    CNA_Bool hasValue = CNA_FALSE;
    const CNA_Result object = cna_content_reader_read_object_tag(reader, &hasValue);
    const CNA_Result shared = cna_content_reader_read_shared_resources(reader);
    CNA_Vector3 vector = {0, 0, 0};
    CNA_Color colour = {0, 0, 0, 0};
    cna_content_reader_read_vector3(reader, &vector);
    cna_content_reader_read_color(reader, &colour);
    CNA_Vector2 past = {7, 7};
    const CNA_Result end = cna_content_reader_read_vector2(reader, &past);
    const CNA_Result limit = cna_content_reader_check_collection_element_count(reader, -1, cprobe_view("Museum.ProbeReader"));
    const CNA_Result readerDestroyed = cna_content_reader_destroy(reader);
    const CNA_Result streamClosed = cna_storage_stream_close(reading);
    cprobe_got(p, "create %u: \"%s\" version %d platform '%c', standalone %s; type readers %u, root object %u (present %u), shared %u, raw (%g, %g, %g) (%u, %u, %u, %u), past the end %u, count -1 -> %u; destroy %u, stream close afterwards %u",
               (unsigned)readerCreated, assetName, (int)version, (char)platform, manager == CNA_INVALID_HANDLE ? "yes" : "no", (unsigned)table, (unsigned)object, hasValue, (unsigned)shared,
               (double)vector.x, (double)vector.y, (double)vector.z, colour.r, colour.g, colour.b, colour.a, (unsigned)end, (unsigned)limit, (unsigned)readerDestroyed, (unsigned)streamClosed);
    CPROBE_CHECK(p, "cna_content_reader_create/copy_asset_name/get_version/get_platform/get_content_manager/initialize_type_readers/read_object_tag/read_shared_resources/read_vector3/read_color/read_vector2/check_collection_element_count/destroy over a storage stream",
                 "the graph reads: Int32Reader resolved, the root object present, no shared resources, then (1, 2, 3) and (10, 20, 30, 40); past the end -> CNA_RESULT_IO; a negative declared count refused; closing the stream afterwards is idempotent",
                 readerCreated == CNA_RESULT_SUCCESS && strcmp(assetName, "graph") == 0 && version == 5 && platform == 'w' && manager == CNA_INVALID_HANDLE && table == CNA_RESULT_SUCCESS &&
                     object == CNA_RESULT_SUCCESS && hasValue == CNA_TRUE && shared == CNA_RESULT_SUCCESS && vector.x == 1.0f && vector.y == 2.0f && vector.z == 3.0f && colour.r == 10 &&
                     colour.a == 40 && end == CNA_RESULT_IO && limit != CNA_RESULT_SUCCESS && readerDestroyed == CNA_RESULT_SUCCESS && streamClosed == CNA_RESULT_SUCCESS);

    uint64_t files = 0;
    cna_storage_container_get_file_name_count(container, cprobe_view("*.xnb"), &files);
    char fileName[64];
    uint64_t fileBytes = 0;
    const CNA_Result listed = cna_storage_container_copy_file_name(container, cprobe_view("*.xnb"), 0u, fileName, sizeof fileName - 1, &fileBytes);
    cprobe_terminate(fileName, sizeof fileName, listed, &fileBytes);
    cna_storage_container_delete_file(container, cprobe_view("graph.xnb"));
    CNA_Bool exists = CNA_TRUE;
    cna_storage_container_file_exists(container, cprobe_view("graph.xnb"), &exists);
    cna_storage_container_delete_directory(container, cprobe_view("saves"));
    const CNA_Result containerDestroyed = cna_storage_container_destroy(container);
    const CNA_Result deleted = cna_storage_device_delete_container(device, cprobe_view("cx-capi C probe"));
    const CNA_Result deviceDestroyed = cna_storage_device_destroy(device);
    char root[1024];
    uint64_t rootBytes = 0;
    const CNA_Result rooted = cna_storage_copy_root_ext(root, sizeof root - 1, &rootBytes);
    cprobe_terminate(root, sizeof root, rooted, &rootBytes);
    const int isolated = g->fixtureHome[0] != '\0' && strncmp(root, g->fixtureHome, strlen(g->fixtureHome)) == 0;
    cprobe_got(p, "%llu *.xnb file(s), \"%s\"; after delete exists %u; container destroy %u, delete container %u, device destroy %u; root %s", (unsigned long long)files, fileName, exists,
               (unsigned)containerDestroyed, (unsigned)deleted, (unsigned)deviceDestroyed, isolated ? "inside the probe's isolated home" : root);
    CPROBE_CHECK(p, "cna_storage_container_get_file_name_count/copy_file_name/delete_file/file_exists/delete_directory/destroy, cna_storage_device_delete_container/destroy, cna_storage_copy_root_ext",
                 "the one graph.xnb listed by the glob and gone after DeleteFile; everything released and the container deleted; the storage root inside the isolated home the museum gave this process",
                 files == 1u && strcmp(fileName, "graph.xnb") == 0 && exists == CNA_FALSE && containerDestroyed == CNA_RESULT_SUCCESS && deleted == CNA_RESULT_SUCCESS &&
                     deviceDestroyed == CNA_RESULT_SUCCESS && isolated);
}

static void GamerServices(GameProbe* g, CNA_Handle game)
{
    CProbe* p = &g->probe;
    CNA_Bool trialBefore = CNA_FALSE, initializedBefore = CNA_TRUE, initialized = CNA_FALSE, trialOff = CNA_TRUE, trialOn = CNA_FALSE;
    cna_guide_get_is_trial_mode(&trialBefore);
    cna_gamer_services_dispatcher_get_is_initialized(&initializedBefore);
    CNA_Bool visibleBefore = CNA_TRUE;
    const CNA_Result visibleRefused = cna_guide_get_is_visible(&visibleBefore);
    const CNA_Result initializedResult = cna_gamer_services_dispatcher_initialize(game);
    cna_gamer_services_dispatcher_get_is_initialized(&initialized);
    cna_guide_set_simulate_trial_mode(CNA_FALSE);
    cna_gamer_services_dispatcher_update(); /* the first update latches the licence state */
    cna_guide_get_is_trial_mode(&trialOff);
    cna_guide_set_simulate_trial_mode(CNA_TRUE);
    CNA_Bool simulated = CNA_FALSE;
    cna_guide_get_simulate_trial_mode(&simulated);
    cna_gamer_services_dispatcher_update();
    cna_guide_get_is_trial_mode(&trialOn);
    cna_guide_set_simulate_trial_mode(CNA_FALSE);
    cna_gamer_services_dispatcher_update();
    cprobe_got(p, "before: trial %u, initialized %u, IsVisible -> %u; initialize %u -> %u; trial after an update with simulation off %u, on (%u) %u", trialBefore, initializedBefore,
               (unsigned)visibleRefused, (unsigned)initializedResult, initialized, trialOff, simulated, trialOn);
    CPROBE_CHECK(p, "cna_guide_get_is_trial_mode/get_is_visible/set+get_simulate_trial_mode, cna_gamer_services_dispatcher_get_is_initialized/initialize/update",
                 "XNA: IsTrialMode true until gamer services first update, IsVisible refused before initialization; afterwards each update latches the simulated trial mode (off, then on)",
                 trialBefore == CNA_TRUE && initializedBefore == CNA_FALSE && visibleRefused == CNA_RESULT_INVALID_STATE && initializedResult == CNA_RESULT_SUCCESS && initialized == CNA_TRUE &&
                     trialOff == CNA_FALSE && simulated == CNA_TRUE && trialOn == CNA_TRUE);

    /* The Guide's on-screen keyboard, cancelled through the ABI's own hook. */
    const CNA_Result shown = cna_guide_begin_show_keyboard_input(CNA_PLAYER_INDEX_ONE, cprobe_view("Name"), cprobe_view("Your museum name"), cprobe_view("Curator"), CNA_FALSE, OnKeyboard, NULL);
    CNA_Bool pending = CNA_FALSE, visible = CNA_FALSE, pendingAfter = CNA_TRUE, canceled = CNA_FALSE;
    cna_guide_get_has_pending_keyboard_input_ext(&pending);
    cna_guide_get_is_visible(&visible);
    const CNA_Result again = cna_guide_begin_show_keyboard_input(CNA_PLAYER_INDEX_ONE, cprobe_view("x"), cprobe_view("x"), cprobe_view(""), CNA_FALSE, NULL, NULL);
    const CNA_Result cancel = cna_guide_simulate_keyboard_input_cancel_ext();
    cna_guide_get_has_pending_keyboard_input_ext(&pendingAfter);
    cna_guide_was_keyboard_input_canceled_ext(&canceled);
    cna_guide_reset_pending_keyboard_input_ext();
    cprobe_got(p, "show %u: pending %u, visible %u, a second while visible %u; cancel %u: callbacks %d, pending %u, canceled %u", (unsigned)shown, pending, visible, (unsigned)again,
               (unsigned)cancel, keyboardCallbacks, pendingAfter, canceled);
    CPROBE_CHECK(p, "cna_guide_begin_show_keyboard_input, get_has_pending_keyboard_input_ext, simulate_keyboard_input_cancel_ext, was_keyboard_input_canceled_ext, reset_pending_keyboard_input_ext",
                 "pending with the Guide visible, a second request refused (INVALID_STATE, as XNA's GuideAlreadyVisibleException); the cancel completes it: the C callback once, nothing pending, WasCanceled",
                 shown == CNA_RESULT_SUCCESS && pending == CNA_TRUE && visible == CNA_TRUE && again == CNA_RESULT_INVALID_STATE && cancel == CNA_RESULT_SUCCESS && keyboardCallbacks == 1 &&
                     pendingAfter == CNA_FALSE && canceled == CNA_TRUE);

    /* A signed-in gamer published to the process, and a local session hosted by it: no peers, no network. */
    CNA_SignedInGamerHandle gamer = CNA_INVALID_HANDLE;
    const CNA_Result gamerCreated = cna_signed_in_gamer_create_ext(cprobe_view("Curator"), CNA_FALSE, CNA_FALSE, CNA_PLAYER_INDEX_TWO, &gamer);
    const CNA_SignedInGamerHandle gamers[1] = {gamer};
    cna_gamer_set_signed_in_gamers_ext(gamers, 1u);
    int32_t signedIn = 0;
    cna_gamer_get_signed_in_gamer_count(&signedIn);
    CNA_Bool atOne = CNA_TRUE, atTwo = CNA_FALSE;
    CNA_SignedInGamerHandle one = CNA_INVALID_HANDLE, two = CNA_INVALID_HANDLE;
    cna_gamer_get_signed_in_gamer_at_player_index(CNA_PLAYER_INDEX_ONE, &atOne, &one);
    cna_gamer_get_signed_in_gamer_at_player_index(CNA_PLAYER_INDEX_TWO, &atTwo, &two);
    char tag[32];
    uint64_t tagBytes = 0;
    const CNA_Result tagged = cna_gamer_copy_gamertag(two, tag, sizeof tag - 1, &tagBytes);
    cprobe_terminate(tag, sizeof tag, tagged, &tagBytes);
    const CNA_Result gamerBusy = cna_signed_in_gamer_destroy(gamer);
    CNA_NetworkSessionHandle session = CNA_INVALID_HANDLE;
    const CNA_Result sessionCreated = cna_network_session_create_with_local_gamers(CNA_NETWORK_SESSION_TYPE_LOCAL, gamers, 1u, 4, 0, CNA_INVALID_HANDLE, &session);
    CNA_NetworkSessionState lobby = 99, playing = 99;
    cna_network_session_get_session_state(session, &lobby);
    CNA_Bool host = CNA_FALSE;
    cna_network_session_get_is_host(session, &host);
    int32_t all = 0, local = 0, maxGamers = 0;
    cna_network_session_get_gamer_count(session, CNA_NETWORK_SESSION_ROSTER_ALL, &all);
    cna_network_session_get_gamer_count(session, CNA_NETWORK_SESSION_ROSTER_LOCAL, &local);
    cna_network_session_get_max_gamers(session, &maxGamers);
    CNA_NetworkSessionHandle second = CNA_INVALID_HANDLE;
    const CNA_Result secondSession = cna_network_session_create(CNA_NETWORK_SESSION_TYPE_LOCAL, 1, 4, &second);
    /* XNA's StartGame queues a transition; the state changes at the next NetworkSession.Update. */
    const CNA_Result started = cna_network_session_start_game(session);
    CNA_NetworkSessionState beforeUpdate = 99;
    cna_network_session_get_session_state(session, &beforeUpdate);
    const CNA_Result updated = cna_network_session_update(session);
    cna_network_session_get_session_state(session, &playing);
    const CNA_Result startedTwice = cna_network_session_start_game(session);
    const CNA_Result sessionDestroyed = cna_network_session_destroy(session);
    cna_gamer_set_signed_in_gamers_ext(NULL, 0u);
    const CNA_Result gamerDestroyed = cna_signed_in_gamer_destroy(gamer);
    cprobe_got(p, "gamer %u, %d signed in, player one %u, player two %u \"%s\", destroy while published %u; session %u: state %u, host %u, %d gamer(s) (%d local), max %d; a second session %u; start %u -> state %u, update %u -> state %u, start again %u; destroy %u, gamer destroy %u",
               (unsigned)gamerCreated, (int)signedIn, atOne, atTwo, tag, (unsigned)gamerBusy, (unsigned)sessionCreated, (unsigned)lobby, host, (int)all, (int)local, (int)maxGamers,
               (unsigned)secondSession, (unsigned)started, (unsigned)beforeUpdate, (unsigned)updated, (unsigned)playing, (unsigned)startedTwice, (unsigned)sessionDestroyed, (unsigned)gamerDestroyed);
    CPROBE_CHECK(p, "cna_signed_in_gamer_create_ext/destroy, cna_gamer_set_signed_in_gamers_ext/get_signed_in_gamer_count/get_signed_in_gamer_at_player_index/copy_gamertag, cna_network_session_create_with_local_gamers/create/get_session_state/get_is_host/get_gamer_count/get_max_gamers/start_game/update/destroy",
                 "one gamer found at player two only (\"Curator\"), not destroyable while published (INVALID_STATE); the local session: Lobby, hosted here, 1 gamer (local), max 4; a second session refused (INVALID_STATE); StartGame queues the transition (still Lobby), Update makes it Playing, another StartGame refused; released cleanly",
                 gamerCreated == CNA_RESULT_SUCCESS && signedIn == 1 && atOne == CNA_FALSE && atTwo == CNA_TRUE && strcmp(tag, "Curator") == 0 && gamerBusy == CNA_RESULT_INVALID_STATE &&
                     sessionCreated == CNA_RESULT_SUCCESS && lobby == CNA_NETWORK_SESSION_STATE_LOBBY && host == CNA_TRUE && all == 1 && local == 1 && maxGamers == 4 &&
                     secondSession == CNA_RESULT_INVALID_STATE && started == CNA_RESULT_SUCCESS && beforeUpdate == CNA_NETWORK_SESSION_STATE_LOBBY && updated == CNA_RESULT_SUCCESS && playing == CNA_NETWORK_SESSION_STATE_PLAYING && startedTwice == CNA_RESULT_INVALID_STATE &&
                     sessionDestroyed == CNA_RESULT_SUCCESS && gamerDestroyed == CNA_RESULT_SUCCESS);
}

void gp_services(GameProbe* g, CNA_Handle game, CNA_Handle device)
{
    (void)device;
    StorageAndReaders(g, game);
    GamerServices(g, game);
}
