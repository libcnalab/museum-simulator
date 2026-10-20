/* The content families of CNA's C ABI that need no device: the .cnb container (cnb.h: checksums, identities, overflow
 * checks, compression, the byte writer and reader, a whole document written and parsed back, a curve encoded and
 * decoded) and the process-wide .xnb type-reader registry (content_readers.h). Expectations come from the header
 * documentation (CRC-32C parameters, BC block sizes, the CNB magic) and from XNA's reader names. */
#include <math.h>

#include "CApi/CProbe.h"
#include "CNA/C/cnb.h"
#include "CNA/C/content_readers.h"
#include "CNA/C/curve.h"

static void ChecksumAndIdentityChecks(CProbe* p)
{
    /* CRC-32C of "123456789" is the standard check value 0xE3069283; split in two it must agree. */
    const uint8_t digits[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    uint32_t whole = 0, portable = 0, first = 0, second = 0;
    cna_cnb_crc32c(digits, sizeof digits, &whole);
    cna_cnb_crc32c_portable(digits, sizeof digits, &portable);
    cna_cnb_crc32c_continue(CNA_CNB_CRC32C_SEED, digits, 4u, &first);
    cna_cnb_crc32c_continue(first, digits + 4, 5u, &second);
    CNA_Bool hardware = CNA_FALSE;
    cna_cnb_crc32c_uses_hardware(&hardware);
    cprobe_got(p, "0x%08X, portable 0x%08X, split 0x%08X (hardware path %u)", (unsigned)whole, (unsigned)portable, (unsigned)second, hardware);
    CPROBE_CHECK(p, "cna_cnb_crc32c / crc32c_portable / crc32c_continue(\"123456789\")", "0xE3069283 three times (the CRC-32C check value)",
                 whole == 0xE3069283u && portable == 0xE3069283u && second == 0xE3069283u);

    uint8_t magic[4] = {0, 0, 0, 0};
    uint64_t magicBytes = 0;
    cna_cnb_copy_format_magic(magic, sizeof magic, &magicBytes);
    CNA_Bool hasMagic = CNA_FALSE, notMagic = CNA_TRUE;
    cna_cnb_has_magic(magic, sizeof magic, &hasMagic);
    cna_cnb_has_magic(digits, sizeof digits, &notMagic);
    CNA_CnbChunkId id = 0;
    cna_cnb_make_chunk_id('m', 'u', 's', '1', &id);
    char idText[8];
    uint64_t idBytes = 0;
    cprobe_terminate(idText, sizeof idText, cna_cnb_copy_chunk_id_string(id, idText, sizeof idText - 1, &idBytes), &idBytes);
    CNA_Bool wellFormed = CNA_FALSE, badFormed = CNA_TRUE;
    cna_cnb_is_well_formed_chunk_id(id, &wellFormed);
    cna_cnb_is_well_formed_chunk_id(0x0A414243u, &badFormed);
    char badText[8];
    uint64_t badBytes = 0;
    cprobe_terminate(badText, sizeof badText, cna_cnb_copy_chunk_id_string(0x0A414243u, badText, sizeof badText - 1, &badBytes), &badBytes);
    cprobe_got(p, "magic %02X %02X %02X %02X (%llu), has_magic %u/%u; chunk id 0x%08X \"%s\" well-formed %u; 0x0A414243 \"%s\" well-formed %u", magic[0], magic[1], magic[2], magic[3],
               (unsigned long long)magicBytes, hasMagic, notMagic, (unsigned)id, idText, wellFormed, badText, badFormed);
    CPROBE_CHECK(p, "cna_cnb_copy_format_magic, has_magic, make_chunk_id, copy_chunk_id_string, is_well_formed_chunk_id",
                 "43 4E 42 1A (\"CNB\\x1A\"), recognised/not; 'm','u','s','1' packs little-endian to 0x3173756D \"mus1\"; a control byte renders as '?' and is not well-formed",
                 magic[0] == 0x43 && magic[1] == 0x4E && magic[2] == 0x42 && magic[3] == 0x1A && magicBytes == 4u && hasMagic == CNA_TRUE && notMagic == CNA_FALSE &&
                     id == 0x3173756Du && strcmp(idText, "mus1") == 0 && wellFormed == CNA_TRUE && strcmp(badText, "CBA?") == 0 && badFormed == CNA_FALSE);

    uint64_t sum = 7, product = 7, fine = 0;
    const CNA_Result addOverflow = cna_cnb_checked_add(UINT64_MAX, 2u, &sum);
    const CNA_Result mulOverflow = cna_cnb_checked_multiply(UINT64_C(0x100000000), UINT64_C(0x100000000), &product);
    const CNA_Result mulFine = cna_cnb_checked_multiply(UINT64_C(0xFFFFFFFF), UINT64_C(0x10000), &fine);
    uint32_t custom = 0;
    cna_cnb_asset_type_id_from_name(cprobe_view("MyGame.Level"), &custom);
    CNA_Bool isCustom = CNA_FALSE;
    cna_cnb_is_custom_asset_type_id(custom, &isCustom);
    uint32_t emptyId = 5;
    const CNA_Result emptyName = cna_cnb_asset_type_id_from_name(cprobe_view(""), &emptyId);
    /* FNV-1a-32 of "MyGame.Level", with the custom bit set. */
    uint32_t fnv = 2166136261u;
    for (const char* c = "MyGame.Level"; *c != '\0'; ++c) fnv = (fnv ^ (uint8_t)*c) * 16777619u;
    cprobe_got(p, "add %u (sum %s), multiply %u (product %s), 0xFFFFFFFF*0x10000 %u = 0x%llX; \"MyGame.Level\" -> 0x%08X custom %u, empty name %u", (unsigned)addOverflow,
               sum == 7 ? "untouched" : "written", (unsigned)mulOverflow, product == 7 ? "untouched" : "written", (unsigned)mulFine, (unsigned long long)fine, (unsigned)custom, isCustom,
               (unsigned)emptyName);
    CPROBE_CHECK(p, "cna_cnb_checked_add/checked_multiply, asset_type_id_from_name, is_custom_asset_type_id",
                 "CNA_RESULT_OVERFLOW twice with the outputs untouched, 0xFFFFFFFF0000; FNV-1a-32(name) | 0x80000000, custom; empty name -> INVALID_ARGUMENT",
                 addOverflow == CNA_RESULT_OVERFLOW && sum == 7 && mulOverflow == CNA_RESULT_OVERFLOW && product == 7 && mulFine == CNA_RESULT_SUCCESS && fine == UINT64_C(0xFFFFFFFF0000) &&
                     custom == (fnv | 0x80000000u) && isCustom == CNA_TRUE && emptyName == CNA_RESULT_INVALID_ARGUMENT);
}

static void CompressionAndTextureChecks(CProbe* p)
{
    CNA_Bool none = CNA_FALSE, bogus = CNA_TRUE;
    cna_cnb_is_compression_supported(CNA_CNB_COMPRESSION_NONE, &none);
    cna_cnb_is_compression_supported(99u, &bogus);
    char name[48];
    uint64_t nameBytes = 0;
    cprobe_terminate(name, sizeof name, cna_cnb_copy_compression_name(99u, name, sizeof name - 1, &nameBytes), &nameBytes);
    const uint8_t raw[6] = {1, 2, 3, 4, 5, 6};
    uint64_t compressedCount = 0, stored = 0, expanded = 0;
    uint8_t packed[16], unpacked[16];
    memset(unpacked, 0, sizeof unpacked);
    cna_cnb_get_compressed_byte_count(raw, sizeof raw, CNA_CNB_COMPRESSION_NONE, 0, &compressedCount);
    cna_cnb_copy_compressed(raw, sizeof raw, CNA_CNB_COMPRESSION_NONE, 0, packed, sizeof packed, &stored);
    const CNA_Result roundTrip = cna_cnb_copy_decompressed(packed, stored, CNA_CNB_COMPRESSION_NONE, sizeof raw, 1024u, unpacked, sizeof unpacked, &expanded);
    uint64_t wrongSize = 0;
    const CNA_Result wrong = cna_cnb_copy_decompressed(packed, stored, CNA_CNB_COMPRESSION_NONE, 5u, 1024u, unpacked, sizeof unpacked, &wrongSize);
    cprobe_got(p, "NONE %u, codec 99 %u \"%s\"; count %llu, stored %llu, round trip %u (%llu bytes, %s); 6 stored bytes declared as 5 -> %u (%llu bytes)", none, bogus, name,
               (unsigned long long)compressedCount, (unsigned long long)stored, (unsigned)roundTrip, (unsigned long long)expanded, memcmp(raw, unpacked, sizeof raw) == 0 ? "same" : "different",
               (unsigned)wrong, (unsigned long long)wrongSize);
    CPROBE_CHECK(p, "cna_cnb_is_compression_supported, copy_compression_name, get_compressed_byte_count, copy_compressed, copy_decompressed",
                 "NONE supported, 99 not and named \"unknown codec 99\"; 6 bytes stored as 6 and restored; a payload that does not expand to exactly the declared size is refused (CNA_RESULT_IO), NONE included",
                 none == CNA_TRUE && bogus == CNA_FALSE && strcmp(name, "unknown codec 99") == 0 && compressedCount == 6u && stored == 6u && roundTrip == CNA_RESULT_SUCCESS &&
                     expanded == 6u && memcmp(raw, unpacked, sizeof raw) == 0 && wrong == CNA_RESULT_IO);

    uint64_t bc7 = 0, bc1 = 0, rgba = 0, zero = 77;
    cna_cnb_get_texture_level_byte_size(CNA_CNB_TEXTURE_FORMAT_BC7, 1u, 1u, 1u, &bc7);
    cna_cnb_get_texture_level_byte_size(CNA_CNB_TEXTURE_FORMAT_BC1, 5u, 5u, 1u, &bc1);
    cna_cnb_get_texture_level_byte_size(CNA_CNB_TEXTURE_FORMAT_RGBA8, 3u, 2u, 1u, &rgba);
    const CNA_Result zeroResult = cna_cnb_get_texture_level_byte_size(CNA_CNB_TEXTURE_FORMAT_RGBA8, 0u, 2u, 1u, &zero);
    CNA_Bool bc1Block = CNA_FALSE, rgbaBlock = CNA_TRUE;
    cna_cnb_is_block_compressed_texture_format(CNA_CNB_TEXTURE_FORMAT_BC1, &bc1Block);
    cna_cnb_is_block_compressed_texture_format(CNA_CNB_TEXTURE_FORMAT_RGBA8, &rgbaBlock);
    cprobe_got(p, "BC7 1x1 %llu, BC1 5x5 %llu, RGBA8 3x2 %llu, zero width %u; BC1 block %u, RGBA8 block %u", (unsigned long long)bc7, (unsigned long long)bc1, (unsigned long long)rgba,
               (unsigned)zeroResult, bc1Block, rgbaBlock);
    CPROBE_CHECK(p, "cna_cnb_get_texture_level_byte_size, is_block_compressed_texture_format", "16 (a whole 4x4 block), 32 (2x2 blocks of 8), 24; zero width -> CNA_RESULT_IO; yes, no",
                 bc7 == 16u && bc1 == 32u && rgba == 24u && zeroResult == CNA_RESULT_IO && bc1Block == CNA_TRUE && rgbaBlock == CNA_FALSE);
}

static void WriterReaderChecks(CProbe* p)
{
    CNA_CnbByteWriterHandle writer = CNA_INVALID_HANDLE;
    cna_cnb_byte_writer_create(&writer);
    cna_cnb_byte_writer_write_u32(writer, 0xDEADBEEFu);
    cna_cnb_byte_writer_write_string(writer, cprobe_view("hello"));
    uint64_t size = 0, copied = 0;
    cna_cnb_byte_writer_get_size(writer, &size);
    uint8_t bytes[32];
    memset(bytes, 0, sizeof bytes);
    cna_cnb_byte_writer_copy_bytes(writer, bytes, sizeof bytes, &copied);
    const CNA_Result writerDestroyed = cna_cnb_byte_writer_destroy(writer);

    CNA_CnbReaderHandle reader = CNA_INVALID_HANDLE;
    const CNA_Result readerCreated = cna_cnb_reader_create(bytes, copied, cprobe_view("museum C probe"), NULL, &reader);
    uint32_t word = 0;
    uint64_t stringBytes = 0, textBytes = 0;
    cna_cnb_reader_read_u32(reader, &word);
    cna_cnb_reader_read_string(reader, &stringBytes);
    char text[16];
    cprobe_terminate(text, sizeof text, cna_cnb_reader_copy_string(reader, text, sizeof text - 1, &textBytes), &textBytes);
    const CNA_Result exhausted = cna_cnb_reader_require_exhausted(reader);
    uint32_t past = 0x12345678u;
    const CNA_Result truncated = cna_cnb_reader_read_u32(reader, &past);
    cna_cnb_reader_destroy(reader);
    cprobe_got(p, "%llu bytes, first EF BE AD DE = %02X %02X %02X %02X; read back 0x%08X \"%s\"; exhausted %u; one more read %u (value %s); writer destroy %u, reader create %u",
               (unsigned long long)size, bytes[0], bytes[1], bytes[2], bytes[3], (unsigned)word, text, (unsigned)exhausted, (unsigned)truncated,
               past == 0x12345678u ? "untouched" : "written", (unsigned)writerDestroyed, (unsigned)readerCreated);
    CPROBE_CHECK(p, "cna_cnb_byte_writer_create/write_u32/write_string/get_size/copy_bytes/destroy, cna_cnb_reader_create/read_u32/read_string/copy_string/require_exhausted",
                 "13 bytes, little-endian EF BE AD DE; 0xDEADBEEF and \"hello\" back; exhausted; reading past the end -> CNA_RESULT_IO with the output untouched",
                 size == 13u && copied == 13u && bytes[0] == 0xEF && bytes[1] == 0xBE && bytes[2] == 0xAD && bytes[3] == 0xDE && word == 0xDEADBEEFu && strcmp(text, "hello") == 0 &&
                     exhausted == CNA_RESULT_SUCCESS && truncated == CNA_RESULT_IO && past == 0x12345678u);

    /* A whole container: one custom chunk, built, then parsed back with its table of contents. */
    CNA_CnbWriterHandle builder = CNA_INVALID_HANDLE;
    CNA_CnbChunkId chunkId = 0;
    cna_cnb_make_chunk_id('m', 'u', 's', '1', &chunkId);
    const CNA_Result builderCreated = cna_cnb_writer_create(CNA_CNB_ASSET_TYPE_CURVE, 1u, &builder);
    cna_cnb_writer_set_metadata(builder, cprobe_view("Microsoft.Xna.Framework.Curve"), cprobe_view("museum/curve"));
    const uint8_t payload[5] = {'C', '1', '7', '!', 0};
    cna_cnb_writer_add_chunk(builder, chunkId, payload, sizeof payload, CNA_CNB_CHUNK_FLAG_NONE, 4u);
    uint64_t needed = 0;
    cna_cnb_writer_build(builder, NULL, 0u, &needed);
    uint8_t image[512];
    uint64_t built = 0;
    const CNA_Result buildResult = cna_cnb_writer_build(builder, image, sizeof image, &built);
    cna_cnb_writer_destroy(builder);
    CNA_CnbDocumentHandle document = CNA_INVALID_HANDLE;
    const CNA_Result parsed = cna_cnb_document_parse(image, built, cprobe_view("museum.cnb"), NULL, &document);
    uint16_t major = 0;
    uint32_t assetType = 0;
    cna_cnb_document_get_container_major(document, &major);
    cna_cnb_document_get_asset_type_id(document, &assetType);
    CNA_Bool found = CNA_FALSE;
    uint64_t index = 99, chunkCount = 0;
    cna_cnb_document_get_chunk_count(document, &chunkCount);
    cna_cnb_document_find_single(document, chunkId, &found, &index);
    CNA_CnbChunkEntry entry;
    memset(&entry, 0, sizeof entry);
    entry.struct_size = (uint32_t)sizeof entry;
    entry.struct_version = CNA_CNB_CHUNK_ENTRY_STRUCT_VERSION;
    cna_cnb_document_get_chunk(document, index, &entry);
    uint8_t chunkData[8];
    uint64_t chunkBytes = 0;
    cna_cnb_document_copy_chunk_data(document, index, chunkData, sizeof chunkData, &chunkBytes);
    uint32_t payloadCrc = 0;
    cna_cnb_crc32c(payload, sizeof payload, &payloadCrc);
    char contentName[32];
    uint64_t contentBytes = 0;
    cprobe_terminate(contentName, sizeof contentName, cna_cnb_document_copy_metadata_content_name(document, contentName, sizeof contentName - 1, &contentBytes), &contentBytes);
    const CNA_Result documentDestroyed = cna_cnb_document_destroy(document);
    image[built / 2] ^= 0xFFu;
    CNA_CnbDocumentHandle corrupt = CNA_INVALID_HANDLE;
    const CNA_Result corruptParse = cna_cnb_document_parse(image, built, cprobe_view("corrupt.cnb"), NULL, &corrupt);
    if (corruptParse == CNA_RESULT_SUCCESS) cna_cnb_document_destroy(corrupt);
    cprobe_got(p, "create %u, size query %llu = build %llu (%u); parse %u: major %u, asset type %u, %llu chunks, found %u at %llu, entry %llu bytes crc 0x%08X (payload 0x%08X), data \"%.4s\", content name \"%s\"; destroy %u; one flipped byte -> %u",
               (unsigned)builderCreated, (unsigned long long)needed, (unsigned long long)built, (unsigned)buildResult, (unsigned)parsed, (unsigned)major, (unsigned)assetType,
               (unsigned long long)chunkCount, found, (unsigned long long)index, (unsigned long long)entry.uncompressed_size, (unsigned)entry.checksum, (unsigned)payloadCrc,
               (const char*)chunkData, contentName, (unsigned)documentDestroyed, (unsigned)corruptParse);
    CPROBE_CHECK(p, "cna_cnb_writer_create/set_metadata/add_chunk/build/destroy, cna_cnb_document_parse/get_container_major/get_asset_type_id/get_chunk_count/find_single/get_chunk/copy_chunk_data/copy_metadata_content_name/destroy",
                 "the image parses back: major 1, asset type CURVE(7), the chunk found with 5 bytes and the CRC-32C of its payload, content name \"museum/curve\"; a flipped byte -> CNA_RESULT_IO",
                 builderCreated == CNA_RESULT_SUCCESS && buildResult == CNA_RESULT_SUCCESS && needed == built && parsed == CNA_RESULT_SUCCESS && major == 1u &&
                     assetType == CNA_CNB_ASSET_TYPE_CURVE && found == CNA_TRUE && entry.uncompressed_size == 5u && entry.checksum == payloadCrc && chunkBytes == 5u &&
                     memcmp(chunkData, payload, 5) == 0 && strcmp(contentName, "museum/curve") == 0 && documentDestroyed == CNA_RESULT_SUCCESS && corruptParse == CNA_RESULT_IO);

    /* A curve through the container: encode, parse, decode, evaluate. */
    CNA_CurveHandle curve = CNA_INVALID_HANDLE, decoded = CNA_INVALID_HANDLE;
    CNA_CurveKeyCollectionHandle keys = CNA_INVALID_HANDLE;
    cna_curve_create(&curve);
    cna_curve_get_keys(curve, &keys);
    CNA_CurveKey key;
    cna_curve_key_init_position_value(0.0f, 2.0f, &key);
    cna_curve_key_collection_add(keys, key);
    cna_curve_key_init_position_value(4.0f, 10.0f, &key);
    cna_curve_key_collection_add(keys, key);
    uint8_t encoded[512];
    uint64_t encodedBytes = 0;
    const CNA_Result encodeResult = cna_cnb_encode_curve(curve, cprobe_view("ramp"), encoded, sizeof encoded, &encodedBytes);
    CNA_CnbDocumentHandle curveDocument = CNA_INVALID_HANDLE;
    cna_cnb_document_parse(encoded, encodedBytes, cprobe_view("ramp.cnb"), NULL, &curveDocument);
    const CNA_Result decodeResult = cna_cnb_decode_curve(curveDocument, &decoded);
    float atOne = -1.0f, originalAtOne = -2.0f;
    cna_curve_evaluate(decoded, 1.0f, &atOne);
    cna_curve_evaluate(curve, 1.0f, &originalAtOne);
    cna_curve_destroy(decoded);
    cna_curve_destroy(curve);
    cna_cnb_document_destroy(curveDocument);
    cprobe_got(p, "encode %u (%llu bytes), decode %u, decoded curve at 1 = %g, original %g", (unsigned)encodeResult, (unsigned long long)encodedBytes, (unsigned)decodeResult, (double)atOne,
               (double)originalAtOne);
    CPROBE_CHECK(p, "cna_cnb_encode_curve, cna_cnb_decode_curve",
                 "the two keys (0,2) and (4,10) with flat tangents survive the round trip: 3.25 at position 1 (XNA's Hermite step 2 + 8 * (3t^2 - 2t^3), t = 0.25), as the original",
                 encodeResult == CNA_RESULT_SUCCESS && decodeResult == CNA_RESULT_SUCCESS && fabsf(atOne - 3.25f) < 1e-6f && atOne == originalAtOne);
}

typedef struct ReaderFactory {
    int created;
    int destroyed;
} ReaderFactory;

static CNA_Result CreateReader(void* context, void** outReaderContext)
{
    ReaderFactory* factory = (ReaderFactory*)context;
    factory->created += 1;
    *outReaderContext = factory;
    return CNA_RESULT_SUCCESS;
}

static CNA_Result ReadNothing(void* readerContext, CNA_ContentReaderHandle input, void* existingObject, void** outObject)
{
    (void)readerContext;
    (void)input;
    (void)existingObject;
    *outObject = NULL;
    return CNA_RESULT_NOT_SUPPORTED;
}

static void DestroyReader(void* readerContext)
{
    ((ReaderFactory*)readerContext)->destroyed += 1;
}

static void TypeReaderChecks(CProbe* p)
{
    /* The registry's extension point: a reader factory written in C, registered, instantiated, refused twice, withdrawn. */
    static ReaderFactory factory;
    memset(&factory, 0, sizeof factory);
    CNA_ContentTypeReaderCallbacks callbacks;
    memset(&callbacks, 0, sizeof callbacks);
    callbacks.struct_size = (uint32_t)sizeof callbacks;
    callbacks.struct_version = 1u;
    callbacks.target_type_name = cprobe_view("Museum.Level");
    callbacks.type_version = 3;
    callbacks.can_deserialize_into_existing_object = CNA_FALSE;
    callbacks.create = CreateReader;
    callbacks.read = ReadNothing;
    callbacks.destroy = DestroyReader;
    callbacks.context = &factory;
    const char* readerName = "Museum.CProbe.LevelReader";
    CNA_Handle registration = CNA_INVALID_HANDLE, duplicate = CNA_INVALID_HANDLE;
    const CNA_Result registered = cna_content_type_reader_manager_register(cprobe_view(readerName), &callbacks, &registration);
    const CNA_Result duplicateResult = cna_content_type_reader_manager_register(cprobe_view(readerName), &callbacks, &duplicate);
    CNA_Bool isRegistered = CNA_FALSE, bogusRegistered = CNA_TRUE;
    cna_content_type_reader_manager_get_is_registered(cprobe_view(readerName), &isRegistered);
    cna_content_type_reader_manager_get_is_registered(cprobe_view("Museum.NoSuchReader"), &bogusRegistered);
    CNA_ContentTypeReaderHandle reader = CNA_INVALID_HANDLE, missing = CNA_INVALID_HANDLE;
    const CNA_Result created = cna_content_type_reader_manager_create_reader(cprobe_view(readerName), &reader);
    const CNA_Result missingResult = cna_content_type_reader_manager_create_reader(cprobe_view("Museum.NoSuchReader"), &missing);
    char target[96];
    uint64_t targetBytes = 0, targetSize = 0;
    cna_content_type_reader_get_target_type_name_size(reader, &targetSize);
    const CNA_Result targetCopied = cna_content_type_reader_copy_target_type_name(reader, target, sizeof target - 1, &targetBytes);
    cprobe_terminate(target, sizeof target, targetCopied, &targetBytes);
    int32_t version = -1;
    cna_content_type_reader_get_type_version(reader, &version);
    CNA_Bool inPlace = CNA_TRUE, acceptsThree = CNA_FALSE, acceptsTwo = CNA_TRUE;
    cna_content_type_reader_get_can_deserialize_into_existing_object(reader, &inPlace);
    cna_content_type_reader_supports_version(reader, 3, &acceptsThree);
    cna_content_type_reader_supports_version(reader, 2, &acceptsTwo);
    const CNA_Result destroyed = cna_content_type_reader_destroy(reader);
    const CNA_Result twice = cna_content_type_reader_destroy(reader);
    const CNA_Result unregistered = cna_content_type_reader_manager_unregister(registration);
    const CNA_Result unregisteredTwice = cna_content_type_reader_manager_unregister(registration);
    CNA_Bool stillRegistered = CNA_TRUE;
    cna_content_type_reader_manager_get_is_registered(cprobe_view(readerName), &stillRegistered);
    cprobe_got(p, "register %u, again %u; registered %u (unknown %u); create %u: target \"%s\" (%llu), version %d, into existing %u, accepts 3/2 %u/%u, factory created %d destroyed %d; unknown -> %u; destroy %u, again %u; unregister %u, again %u, registered after %u",
               (unsigned)registered, (unsigned)duplicateResult, isRegistered, bogusRegistered, (unsigned)created, target, (unsigned long long)targetSize, (int)version, inPlace, acceptsThree,
               acceptsTwo, factory.created, factory.destroyed, (unsigned)missingResult, (unsigned)destroyed, (unsigned)twice, (unsigned)unregistered, (unsigned)unregisteredTwice, stillRegistered);
    CPROBE_CHECK(p, "cna_content_type_reader_manager_register/get_is_registered/create_reader/unregister, cna_content_type_reader_get_target_type_name_size/copy_target_type_name/get_type_version/get_can_deserialize_into_existing_object/supports_version/destroy",
                 "a C factory registers once (a duplicate -> INVALID_STATE), creates one reader for \"Museum.Level\" version 3 that accepts 3 but not 2, destroys it through the C callback; unknown name -> NOT_SUPPORTED; second destroy/unregister -> INVALID_HANDLE; gone afterwards",
                 registered == CNA_RESULT_SUCCESS && duplicateResult == CNA_RESULT_INVALID_STATE && isRegistered == CNA_TRUE && bogusRegistered == CNA_FALSE && created == CNA_RESULT_SUCCESS &&
                     strcmp(target, "Museum.Level") == 0 && targetSize == 12u && version == 3 && inPlace == CNA_FALSE && acceptsThree == CNA_TRUE && acceptsTwo == CNA_FALSE &&
                     factory.created == 1 && factory.destroyed == 1 && missingResult == CNA_RESULT_NOT_SUPPORTED && destroyed == CNA_RESULT_SUCCESS && twice == CNA_RESULT_INVALID_HANDLE &&
                     unregistered == CNA_RESULT_SUCCESS && unregisteredTwice == CNA_RESULT_INVALID_HANDLE && stillRegistered == CNA_FALSE);

    char reflective[160], enumName[160];
    uint64_t reflectiveBytes = 0, enumBytes = 0, reflectiveSize = 0, enumSize = 0;
    const char* qualified = "MyGame.Level";
    cna_reflective_type_reader_get_canonical_name_size(cprobe_view(qualified), &reflectiveSize);
    cprobe_terminate(reflective, sizeof reflective, cna_reflective_type_reader_copy_canonical_name(cprobe_view(qualified), reflective, sizeof reflective - 1, &reflectiveBytes), &reflectiveBytes);
    cna_enum_type_reader_get_canonical_name_size(cprobe_view("MyGame.Weapon"), &enumSize);
    cprobe_terminate(enumName, sizeof enumName, cna_enum_type_reader_copy_canonical_name(cprobe_view("MyGame.Weapon"), enumName, sizeof enumName - 1, &enumBytes), &enumBytes);
    CNA_ContentTypeReaderHandle placeholder = CNA_INVALID_HANDLE;
    const CNA_Result placeholderResult = cna_known_unsupported_content_type_reader_create(cprobe_view("Museum.Shader"), CNA_UNSUPPORTED_CONTENT_READER_REASON_COMPILED_PLATFORM_SHADER_BYTECODE, &placeholder);
    char placeholderTarget[64];
    uint64_t placeholderBytes = 0;
    cprobe_terminate(placeholderTarget, sizeof placeholderTarget, cna_content_type_reader_copy_target_type_name(placeholder, placeholderTarget, sizeof placeholderTarget - 1, &placeholderBytes), &placeholderBytes);
    cna_content_type_reader_destroy(placeholder);
    CNA_ContentTypeReaderHandle badReason = CNA_INVALID_HANDLE;
    const CNA_Result badReasonResult = cna_known_unsupported_content_type_reader_create(cprobe_view("Museum.Shader"), 999u, &badReason);
    cprobe_got(p, "reflective \"%s\" (%llu), enum \"%s\" (%llu); placeholder %u targets \"%s\"; reason 999 -> %u", reflective, (unsigned long long)reflectiveSize, enumName,
               (unsigned long long)enumSize, (unsigned)placeholderResult, placeholderTarget, (unsigned)badReasonResult);
    CPROBE_CHECK(p, "cna_reflective_type_reader_get_canonical_name_size/copy_canonical_name, cna_enum_type_reader_get_canonical_name_size/copy_canonical_name, cna_known_unsupported_content_type_reader_create",
                 "\"Microsoft.Xna.Framework.Content.ReflectiveReader`1[[MyGame.Level]]\" and \"Microsoft.Xna.Framework.Content.EnumReader`1[[MyGame.Weapon]]\"; the placeholder targets \"Museum.Shader\"; an unknown reason -> INVALID_ARGUMENT",
                 strcmp(reflective, "Microsoft.Xna.Framework.Content.ReflectiveReader`1[[MyGame.Level]]") == 0 && reflectiveSize == reflectiveBytes &&
                     strcmp(enumName, "Microsoft.Xna.Framework.Content.EnumReader`1[[MyGame.Weapon]]") == 0 && enumSize == enumBytes && placeholderResult == CNA_RESULT_SUCCESS &&
                     strcmp(placeholderTarget, "Museum.Shader") == 0 && badReasonResult == CNA_RESULT_INVALID_ARGUMENT);
}

void museum_c_probe_content(CProbe* probe)
{
    ChecksumAndIdentityChecks(probe);
    CompressionAndTextureChecks(probe);
    WriterReaderChecks(probe);
    TypeReaderChecks(probe);
}
