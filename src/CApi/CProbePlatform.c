/* Platform value families of CNA's C ABI that need no game and change no process-wide state: sensor readings
 * (sensors.h), the device descriptors and environment (devices.h), the GamerServices value objects (gamer_services.h:
 * property dictionaries, achievements, presence, leaderboard identities, avatar descriptions) and the networking
 * value types (net.h packet writer/reader, session properties, quality of service; net_gamers.h gamers and event
 * descriptions; net_sessions.h discovered sessions). Sessions, signed-in gamers, sensors and devices that need a
 * game run in the C game probe. */
#include <math.h>

#include "CApi/CProbe.h"
#include "CNA/C/devices.h"
#include "CNA/C/gamer_services.h"
#include "CNA/C/net.h"
#include "CNA/C/net_gamers.h"
#include "CNA/C/net_sessions.h"
#include "CNA/C/sensors.h"

#define VERSIONED(value)                              \
    do {                                              \
        memset(&(value), 0, sizeof(value));           \
        (value).struct_size = (uint32_t)sizeof(value); \
        (value).struct_version = 1u;                  \
    } while (0)

static void SensorReadingChecks(CProbe* p)
{
    /* The same instant written with two offsets: canonical equality and hashing compare UTC (ticks - offset). */
    const int64_t hour = INT64_C(36000000000);
    CNA_DateTimeOffset utc = {INT64_C(638000000000000000), 0};
    CNA_DateTimeOffset plusOne = {INT64_C(638000000000000000) + hour, hour};
    CNA_DateTimeOffset later = {INT64_C(638000000000000000) + 1, 0};
    const CNA_Vector3 g = {0.0f, 0.0f, -1.0f};
    CNA_AccelerometerReading a, b, c, zero;
    VERSIONED(a);
    VERSIONED(b);
    VERSIONED(c);
    VERSIONED(zero);
    cna_accelerometer_reading_init_from_values(utc, g, &a);
    cna_accelerometer_reading_init_from_values(plusOne, g, &b);
    cna_accelerometer_reading_init_from_values(later, g, &c);
    zero.acceleration.x = 5.0f;
    cna_accelerometer_reading_init(&zero);
    CNA_Bool sameInstant = CNA_FALSE, otherInstant = CNA_TRUE;
    cna_accelerometer_reading_equals(&a, &b, &sameInstant);
    cna_accelerometer_reading_equals(&a, &c, &otherInstant);
    uint64_t hashA = 0, hashB = 1;
    cna_accelerometer_reading_get_hash_code(&a, &hashA);
    cna_accelerometer_reading_get_hash_code(&b, &hashB);
    char typeName[96], text[160];
    uint64_t typeBytes = 0, textBytes = 0, textSize = 0, typeSize = 0;
    cna_accelerometer_reading_get_type_name_size(&typeSize);
    cprobe_terminate(typeName, sizeof typeName, cna_accelerometer_reading_copy_type_name(typeName, sizeof typeName - 1, &typeBytes), &typeBytes);
    cna_accelerometer_reading_get_string_size(&a, &textSize);
    cprobe_terminate(text, sizeof text, cna_accelerometer_reading_copy_string(&a, text, sizeof text - 1, &textBytes), &textBytes);
    cprobe_got(p, "same instant (+1 h offset) equal %u, hashes %s, one tick later equal %u; default (%g, %g, %g); type \"%s\" (%llu); text %llu bytes = copy %llu", sameInstant,
               hashA == hashB ? "equal" : "different", otherInstant, (double)zero.acceleration.x, (double)zero.acceleration.y, (double)zero.acceleration.z, typeName,
               (unsigned long long)typeSize, (unsigned long long)textSize, (unsigned long long)textBytes);
    CPROBE_CHECK(p, "cna_accelerometer_reading_init/init_from_values/equals/get_hash_code/get_type_name_size/copy_type_name/get_string_size/copy_string",
                 "equal and same hash for one UTC instant written with offsets 0 and +1 h, unequal one tick later; zeroed default; \"Microsoft.Devices.Sensors.AccelerometerReading\"",
                 sameInstant == CNA_TRUE && hashA == hashB && otherInstant == CNA_FALSE && zero.acceleration.x == 0.0f && strcmp(typeName, "Microsoft.Devices.Sensors.AccelerometerReading") == 0 &&
                     typeSize == typeBytes && textSize == textBytes && textBytes > 0u);

    CNA_CompassReading north, south;
    VERSIONED(north);
    VERSIONED(south);
    const CNA_Vector3 field = {20.0f, 0.0f, -40.0f};
    cna_compass_reading_init_from_values(5.0, 90.0, field, utc, 92.5, &north);
    cna_compass_reading_init_from_values(5.0, 270.0, field, utc, 272.5, &south);
    CNA_Bool compassEqual = CNA_TRUE;
    cna_compass_reading_equals(&north, &south, &compassEqual);
    char compassType[96];
    uint64_t compassTypeBytes = 0;
    cprobe_terminate(compassType, sizeof compassType, cna_compass_reading_copy_type_name(compassType, sizeof compassType - 1, &compassTypeBytes), &compassTypeBytes);
    cprobe_got(p, "fields: accuracy %g, magnetic %g, true %g, magnetometer (%g, %g, %g); 90 vs 270 equal %u; \"%s\"", north.heading_accuracy, north.magnetic_heading, north.true_heading,
               (double)north.magnetometer_reading.x, (double)north.magnetometer_reading.y, (double)north.magnetometer_reading.z, compassEqual, compassType);
    CPROBE_CHECK(p, "cna_compass_reading_init_from_values/equals/copy_type_name", "the constructor arguments land in their fields; different headings differ; \"Microsoft.Devices.Sensors.CompassReading\"",
                 north.heading_accuracy == 5.0 && north.magnetic_heading == 90.0 && north.true_heading == 92.5 && north.magnetometer_reading.z == -40.0f && compassEqual == CNA_FALSE &&
                     strcmp(compassType, "Microsoft.Devices.Sensors.CompassReading") == 0);
}

static void DeviceValueChecks(CProbe* p)
{
    CNA_DeviceType type = 99;
    const CNA_Result typeResult = cna_environment_get_device_type(&type);
    CNA_Bool extended = CNA_FALSE;
    cna_devices_ext_is_available(&extended);
    CNA_CameraDeviceInfo camera;
    VERSIONED(camera);
    camera.position = CNA_CAMERA_POSITION_BACK_FACING;
    cna_camera_device_info_init(&camera);
    cprobe_got(p, "device type %u (%u), extended layer %u, camera default position %u", (unsigned)type, (unsigned)typeResult, extended, (unsigned)camera.position);
    CPROBE_CHECK(p, "cna_environment_get_device_type, cna_devices_ext_is_available, cna_camera_device_info_init",
                 "EMULATOR(1) on a desktop (CNA reports Device only on Android and iOS, see Microsoft/Devices/Environment.hpp), the extended layer built in (CNA_DEVICES=ON), camera position UNKNOWN(0)",
                 typeResult == CNA_RESULT_SUCCESS && type == CNA_DEVICE_TYPE_EMULATOR && extended == CNA_TRUE && camera.position == CNA_CAMERA_POSITION_UNKNOWN);
}

static void GamerValueChecks(CProbe* p)
{
    CNA_PropertyDictionaryHandle dictionary = CNA_INVALID_HANDLE;
    cna_property_dictionary_create_ext(&dictionary);
    cna_property_dictionary_set_int32(dictionary, cprobe_view("score"), 4200);
    cna_property_dictionary_set_string(dictionary, cprobe_view("motto"), cprobe_view("C17"));
    int32_t count = 0, score = 0;
    cna_property_dictionary_get_count(dictionary, &count);
    cna_property_dictionary_get_int32(dictionary, cprobe_view("score"), &score);
    char motto[16];
    uint64_t mottoBytes = 0;
    cprobe_terminate(motto, sizeof motto, cna_property_dictionary_copy_string(dictionary, cprobe_view("motto"), motto, sizeof motto - 1, &mottoBytes), &mottoBytes);
    CNA_Bool found = CNA_FALSE, hasMissing = CNA_TRUE;
    CNA_PropertyValueKind kind = 0;
    cna_property_dictionary_try_get_value_kind_ext(dictionary, cprobe_view("motto"), &found, &kind);
    cna_property_dictionary_contains_key(dictionary, cprobe_view("missing"), &hasMissing);
    int64_t ticks = 0;
    const CNA_Result wrongKind = cna_property_dictionary_get_date_time_ticks(dictionary, cprobe_view("score"), &ticks);
    int32_t absent = 0;
    const CNA_Result unknownKey = cna_property_dictionary_get_int32(dictionary, cprobe_view("missing"), &absent);
    cna_property_dictionary_destroy(dictionary);
    cprobe_got(p, "%d entries, score %d, motto \"%s\", motto kind found %u/%u, contains missing %u; score as date-time %u, missing key %u", (int)count, (int)score, motto, found, (unsigned)kind,
               hasMissing, (unsigned)wrongKind, (unsigned)unknownKey);
    CPROBE_CHECK(p, "cna_property_dictionary_create_ext/set_int32/set_string/get_count/get_int32/copy_string/try_get_value_kind_ext/contains_key/get_date_time_ticks/destroy",
                 "2 entries, 4200, \"C17\", STRING(8); a slot of another kind -> INVALID_STATE, an unknown key -> INVALID_ARGUMENT",
                 count == 2 && score == 4200 && strcmp(motto, "C17") == 0 && found == CNA_TRUE && kind == CNA_PROPERTY_VALUE_KIND_STRING && hasMissing == CNA_FALSE &&
                     wrongKind == CNA_RESULT_INVALID_STATE && unknownKey == CNA_RESULT_INVALID_ARGUMENT);

    CNA_AchievementHandle first = CNA_INVALID_HANDLE, second = CNA_INVALID_HANDLE, byKey = CNA_INVALID_HANDLE;
    cna_achievement_create_ext(cprobe_view("first-room"), cprobe_view("First Room"), cprobe_view("Enter a room"), CNA_TRUE, CNA_TRUE, INT64_C(630000000000000000), &first);
    cna_achievement_create_ext(cprobe_view("all-floors"), cprobe_view("All Floors"), cprobe_view("Visit every floor"), CNA_FALSE, CNA_FALSE, 0, &second);
    CNA_AchievementCollectionHandle collection = CNA_INVALID_HANDLE;
    const CNA_AchievementHandle both[1] = {first};
    cna_achievement_collection_create_ext(both, 1u, &collection);
    cna_achievement_collection_add(collection, second);
    int32_t achievements = 0;
    cna_achievement_collection_get_count(collection, &achievements);
    const CNA_Result keyed = cna_achievement_collection_get_by_key(collection, cprobe_view("all-floors"), &byKey);
    char name[32];
    uint64_t nameBytes = 0;
    cprobe_terminate(name, sizeof name, cna_achievement_copy_name(byKey, name, sizeof name - 1, &nameBytes), &nameBytes);
    CNA_AchievementInfo info;
    VERSIONED(info);
    cna_achievement_get_info(first, &info);
    CNA_AchievementHandle none = CNA_INVALID_HANDLE;
    const CNA_Result missing = cna_achievement_collection_get_by_key(collection, cprobe_view("nope"), &none);
    char missingMessage[160], missingType[96];
    cprobe_last_message(missingMessage, sizeof missingMessage);
    uint64_t missingTypeBytes = 0;
    const CNA_Result missingTypeCopied = cna_error_copy_last_exception_type_ext(missingType, sizeof missingType - 1, &missingTypeBytes);
    cprobe_terminate(missingType, sizeof missingType, missingTypeCopied, &missingTypeBytes);
    cna_achievement_destroy(byKey);
    cna_achievement_collection_destroy(collection);
    cna_achievement_destroy(first);
    cna_achievement_destroy(second);
    CNA_GamerPresence presence;
    VERSIONED(presence);
    presence.presence_value = 9;
    cna_gamer_presence_init(&presence);
    CNA_LeaderboardIdentity identity;
    VERSIONED(identity);
    cna_leaderboard_identity_init(CNA_LEADERBOARD_KEY_BEST_TIME_RECENT, 2, &identity);
    cprobe_got(p, "%d achievements, by key %u \"%s\", first earned %u shown early %u ticks %lld; presence mode %u value %d; leaderboard \"%s\" mode %d", (int)achievements,
               (unsigned)keyed, name, info.is_earned, info.display_before_earned, (long long)info.earned_date_time_ticks, (unsigned)presence.presence_mode,
               (int)presence.presence_value, identity.key, (int)identity.game_mode);
    CPROBE_CHECK(p, "cna_achievement_create_ext/get_info/copy_name/destroy, cna_achievement_collection_create_ext/add/get_count/get_by_key/destroy, cna_gamer_presence_init, cna_leaderboard_identity_init",
                 "2 achievements, \"all-floors\" -> \"All Floors\", the first earned and shown early with its ticks; presence NONE/0; key \"BestTimeRecent\" (XNA LeaderboardKey.ToString) mode 2",
                 achievements == 2 && keyed == CNA_RESULT_SUCCESS && strcmp(name, "All Floors") == 0 && info.is_earned == CNA_TRUE && info.display_before_earned == CNA_TRUE &&
                     info.earned_date_time_ticks == INT64_C(630000000000000000) && presence.presence_mode == CNA_GAMER_PRESENCE_MODE_NONE && presence.presence_value == 0 &&
                     strcmp(identity.key, "BestTimeRecent") == 0 && identity.game_mode == 2);
    cprobe_got(p, "result %u, exception %s, \"%.90s\"", (unsigned)missing, missingType, missingMessage);
    CPROBE_CHECK(p, "cna_achievement_collection_get_by_key with a key no achievement has", "CNA_RESULT_INVALID_ARGUMENT, as achievement_collection_get_by_key documents",
                 missing == CNA_RESULT_INVALID_ARGUMENT);

    CNA_AvatarDescriptionHandle avatar = CNA_INVALID_HANDLE;
    const CNA_Result avatarResult = cna_avatar_description_create_random(&avatar);
    CNA_AvatarDescriptionInfo avatarInfo;
    VERSIONED(avatarInfo);
    cna_avatar_description_get_info(avatar, &avatarInfo);
    const CNA_Result avatarDestroyed = cna_avatar_description_destroy(avatar);
    cprobe_got(p, "%u: body %u, height %g, %llu description bytes, valid %u; destroy %u", (unsigned)avatarResult, (unsigned)avatarInfo.body_type, (double)avatarInfo.height,
               (unsigned long long)avatarInfo.description_byte_count, avatarInfo.is_valid, (unsigned)avatarDestroyed);
    CPROBE_CHECK(p, "cna_avatar_description_create_random/get_info/destroy", "a valid description with a female or male body and a non-empty byte description",
                 avatarResult == CNA_RESULT_SUCCESS && avatarInfo.is_valid == CNA_TRUE && avatarInfo.body_type <= CNA_AVATAR_BODY_TYPE_MAXIMUM && avatarInfo.description_byte_count > 0u &&
                     avatarDestroyed == CNA_RESULT_SUCCESS);
}

static void NetValueChecks(CProbe* p)
{
    /* XNA's PacketWriter is a BinaryWriter: a Vector3 is 12 bytes, a Color its 4 bytes R G B A, a Single 4, a Double 8. */
    CNA_PacketWriterHandle writer = CNA_INVALID_HANDLE;
    const CNA_Result negative = cna_packet_writer_create(-1, &writer);
    cna_packet_writer_create(64, &writer);
    const CNA_Vector3 v = {1.5f, -2.0f, 8.0f};
    cna_packet_writer_write_vector3(writer, v);
    cna_packet_writer_write_color(writer, (CNA_Color){10, 20, 30, 40});
    cna_packet_writer_write_single(writer, 0.25f);
    cna_packet_writer_write_double(writer, -1.0 / 3.0);
    int32_t length = 0;
    cna_packet_writer_get_length(writer, &length);
    uint8_t bytes[64];
    uint64_t copied = 0;
    cna_packet_writer_copy_data_ext(writer, bytes, sizeof bytes, &copied);
    cna_packet_writer_destroy(writer);
    CNA_PacketReaderHandle reader = CNA_INVALID_HANDLE;
    cna_packet_reader_create(0, &reader);
    cna_packet_reader_set_data_ext(reader, bytes, copied);
    CNA_Vector3 back = {0, 0, 0};
    CNA_Color color = {0, 0, 0, 0};
    float single = 0.0f;
    double dbl = 0.0;
    cna_packet_reader_read_vector3(reader, &back);
    cna_packet_reader_read_color(reader, &color);
    cna_packet_reader_read_single(reader, &single);
    cna_packet_reader_read_double(reader, &dbl);
    int32_t position = 0;
    cna_packet_reader_get_position(reader, &position);
    float past = 7.0f;
    const CNA_Result end = cna_packet_reader_read_single(reader, &past);
    cna_packet_reader_destroy(reader);
    cprobe_got(p, "negative capacity %u; %d bytes, colour bytes at 12: %u %u %u %u; read back (%g, %g, %g) (%u, %u, %u, %u) %g %.17g, position %d, past the end %u", (unsigned)negative,
               (int)length, bytes[12], bytes[13], bytes[14], bytes[15], (double)back.x, (double)back.y, (double)back.z, color.r, color.g, color.b, color.a, (double)single, dbl, (int)position,
               (unsigned)end);
    CPROBE_CHECK(p, "cna_packet_writer_create/write_vector3/write_color/write_single/write_double/get_length/copy_data_ext/destroy, cna_packet_reader_create/set_data_ext/read_*/get_position/destroy",
                 "negative capacity refused; 28 bytes with the colour as 10 20 30 40 at offset 12; every value back exactly; position 28; reading past the end -> CNA_RESULT_IO",
                 negative == CNA_RESULT_INVALID_ARGUMENT && length == 28 && copied == 28u && bytes[12] == 10 && bytes[13] == 20 && bytes[14] == 30 && bytes[15] == 40 && back.x == 1.5f &&
                     back.y == -2.0f && back.z == 8.0f && color.r == 10 && color.a == 40 && single == 0.25f && dbl == -1.0 / 3.0 && position == 28 && end == CNA_RESULT_IO);

    CNA_NetworkSessionPropertiesHandle properties = CNA_INVALID_HANDLE;
    cna_network_session_properties_create(&properties);
    int32_t slots = 0;
    cna_network_session_properties_get_count(properties, &slots);
    CNA_OptionalInt32 empty, set, value = {CNA_TRUE, {0, 0, 0}, 77};
    memset(&empty, 0x5A, sizeof empty);
    cna_network_session_properties_get_item(properties, 3, &empty);
    cna_network_session_properties_set_item(properties, 3, value);
    memset(&set, 0, sizeof set);
    cna_network_session_properties_get_item(properties, 3, &set);
    CNA_Bool contains = CNA_FALSE, readOnly = CNA_TRUE;
    cna_network_session_properties_contains(properties, value, &contains);
    cna_network_session_properties_get_is_read_only(properties, &readOnly);
    const CNA_Result outOfRange = cna_network_session_properties_set_item(properties, 8, value);
    CNA_QualityOfService qos;
    VERSIONED(qos);
    cna_quality_of_service_init_measured(INT64_C(150000), &qos);
    CNA_AvailableNetworkSessionCreateInfo createInfo;
    VERSIONED(createInfo);
    createInfo.current_gamer_count = 1;
    createInfo.open_public_gamer_slots = 3;
    createInfo.session_type = CNA_NETWORK_SESSION_TYPE_SYSTEM_LINK;
    createInfo.host_port = 31337;
    createInfo.host_gamertag = cprobe_view("Curator");
    createInfo.host_address = cprobe_view("127.0.0.1");
    createInfo.session_properties = properties;
    CNA_AvailableNetworkSessionHandle available = CNA_INVALID_HANDLE;
    const CNA_Result availableResult = cna_available_network_session_create_ext(&createInfo, &qos, &available);
    int32_t openPublic = 0;
    cna_available_network_session_get_open_public_gamer_slots(available, &openPublic);
    char host[32];
    uint64_t hostBytes = 0;
    cprobe_terminate(host, sizeof host, cna_available_network_session_copy_host_gamertag(available, host, sizeof host - 1, &hostBytes), &hostBytes);
    cna_available_network_session_destroy(available);
    cna_network_session_properties_destroy(properties);
    cprobe_got(p, "%d slots, slot 3 before %u/%d after %u/%d, contains %u, read-only %u, slot 8 %u; QoS available %u avg %lld min %lld; discovered %u: %d public slots, host \"%s\"", (int)slots,
               empty.has_value, (int)empty.value, set.has_value, (int)set.value, contains, readOnly, (unsigned)outOfRange, qos.is_available, (long long)qos.average_roundtrip_ticks,
               (long long)qos.minimum_roundtrip_ticks, (unsigned)availableResult, (int)openPublic, host);
    CPROBE_CHECK(p, "cna_network_session_properties_create/get_count/get_item/set_item/contains/get_is_read_only/destroy, cna_quality_of_service_init_measured, cna_available_network_session_create_ext/get_open_public_gamer_slots/copy_host_gamertag/destroy",
                 "8 unset slots (XNA NetworkSessionProperties), slot 3 = 77 afterwards, not read-only, slot 8 refused; one sample sets average = minimum = 150000; 3 public slots, host \"Curator\"",
                 slots == 8 && empty.has_value == CNA_FALSE && empty.value == 0 && set.has_value == CNA_TRUE && set.value == 77 && contains == CNA_TRUE && readOnly == CNA_FALSE &&
                     outOfRange == CNA_RESULT_INVALID_ARGUMENT && qos.is_available == CNA_TRUE && qos.average_roundtrip_ticks == INT64_C(150000) &&
                     qos.minimum_roundtrip_ticks == INT64_C(150000) && availableResult == CNA_RESULT_SUCCESS && openPublic == 3 && strcmp(host, "Curator") == 0);

    CNA_NetworkGamerHandle gamer = CNA_INVALID_HANDLE;
    const CNA_Result gamerResult = cna_network_gamer_create(CNA_INVALID_HANDLE, cprobe_view(""), &gamer);
    CNA_Bool host1 = CNA_TRUE, ready = CNA_FALSE;
    cna_network_gamer_get_is_host(gamer, &host1);
    const CNA_Result readyResult = cna_network_gamer_set_is_ready(gamer, CNA_TRUE);
    cna_network_gamer_get_is_ready(gamer, &ready);
    CNA_NetworkGamerHandle badSession = CNA_INVALID_HANDLE;
    const CNA_Result badSessionResult = cna_network_gamer_create((CNA_Handle)UINT64_C(0x0000000100000001), cprobe_view("x"), &badSession);
    cna_network_gamer_destroy(gamer);
    CNA_GameStartedEventInfo started;
    VERSIONED(started);
    const CNA_Result startedResult = cna_game_started_event_info_init(&started);
    CNA_GameStartedEventInfo badStructure;
    memset(&badStructure, 0, sizeof badStructure);
    const CNA_Result badStructureResult = cna_game_started_event_info_init(&badStructure);
    cprobe_got(p, "gamer %u: host %u, set_is_ready %u, ready %u; bogus session handle -> %u; game-started info %u, zero struct_size -> %u", (unsigned)gamerResult, host1, (unsigned)readyResult, ready,
               (unsigned)badSessionResult, (unsigned)startedResult, (unsigned)badStructureResult);
    CPROBE_CHECK(p, "cna_network_gamer_create/get_is_host/set_is_ready/get_is_ready/destroy, cna_game_started_event_info_init",
                 "a session-less gamer is not the host and, not being local, cannot be made ready (INVALID_STATE, as XNA's NetworkGamer.IsReady setter refuses); a handle that is not a session -> INVALID_HANDLE; an uninitialised versioned structure -> INVALID_ARGUMENT",
                 gamerResult == CNA_RESULT_SUCCESS && host1 == CNA_FALSE && readyResult == CNA_RESULT_INVALID_STATE && ready == CNA_FALSE && badSessionResult == CNA_RESULT_INVALID_HANDLE && startedResult == CNA_RESULT_SUCCESS &&
                     badStructureResult == CNA_RESULT_INVALID_ARGUMENT);
}

void museum_c_probe_platform(CProbe* probe)
{
    SensorReadingChecks(probe);
    DeviceValueChecks(probe);
}

void museum_c_probe_gamer(CProbe* probe)
{
    GamerValueChecks(probe);
    NetValueChecks(probe);
}
