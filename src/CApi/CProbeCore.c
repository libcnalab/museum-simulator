/* core_ext.h (CNA's own process-wide services: the logger and its sink, the platform identity, the renderer
 * identities and the latched selection, the product title) and core.h's canonical-exception diagnostics, used from
 * C17 inside the running museum. Process-wide state the checks change (log level, sink, title) is put back. */
#include "CApi/CProbe.h"
#include "CNA/C/core_ext.h"
#include "CNA/C/graphics.h"
#include "CNA/C/vectors.h"

typedef struct SinkCapture {
    int lines;
    CNA_LogLevel level;
    CNA_LogCategory category;
    char text[200];
} SinkCapture;

static void CaptureLine(CNA_LogLevel level, CNA_LogCategory category, CNA_StringView message, void* context)
{
    SinkCapture* capture = (SinkCapture*)context;
    capture->lines += 1;
    capture->level = level;
    capture->category = category;
    const size_t n = message.byte_length < sizeof capture->text - 1 ? (size_t)message.byte_length : sizeof capture->text - 1;
    memcpy(capture->text, message.data, n);
    capture->text[n] = '\0';
}

static int EndsWith(const char* text, const char* suffix)
{
    const size_t a = strlen(text), b = strlen(suffix);
    return a >= b && strcmp(text + a - b, suffix) == 0;
}

static void LoggerChecks(CProbe* p)
{
    CNA_LogLevel saved = CNA_LOG_LEVEL_INFO, readBack = 99;
    cna_logger_get_minimum_level(&saved);
    SinkCapture capture;
    memset(&capture, 0, sizeof capture);
    const CNA_Result sinkResult = cna_logger_set_sink_ext(CaptureLine, &capture);
    cna_logger_set_minimum_level(CNA_LOG_LEVEL_WARN);
    cna_logger_get_minimum_level(&readBack);
    /* Below the minimum (dropped), a false condition (dropped), then one warning that gets through. */
    cna_logger_info(cprobe_view("museum C probe: info below the minimum"), CNA_LOG_CATEGORY_APPLICATION);
    cna_logger_warn_if(cprobe_view("museum C probe: condition false"), CNA_FALSE);
    cna_logger_log(CNA_LOG_LEVEL_ERROR, cprobe_view("museum C probe: explicit condition false"), CNA_LOG_CATEGORY_TEST, CNA_FALSE);
    const int beforeWarn = capture.lines;
    cna_logger_warn(cprobe_view("museum C probe: one warning"), CNA_LOG_CATEGORY_APPLICATION);
    const SinkCapture afterWarn = capture;
    const CNA_Result badLevel = cna_logger_log((CNA_LogLevel)42u, cprobe_view("x"), CNA_LOG_CATEGORY_APPLICATION, CNA_TRUE);
    cna_logger_set_minimum_level(saved);
    const CNA_Result resetResult = cna_logger_reset_sink_ext();
    cna_logger_info(cprobe_view("museum C probe: after reset (stderr, not the C sink)"), CNA_LOG_CATEGORY_APPLICATION);
    cprobe_got(p, "sink %u, level read back %u, %d lines before the warning, then %d: level %u category %u \"%s\"; level 42 -> %u; reset %u, lines after reset %d",
               (unsigned)sinkResult, (unsigned)readBack, beforeWarn, afterWarn.lines, (unsigned)afterWarn.level, (unsigned)afterWarn.category, afterWarn.text, (unsigned)badLevel,
               (unsigned)resetResult, capture.lines);
    CPROBE_CHECK(p, "cna_logger_set_sink_ext, set/get_minimum_level(WARN), info/warn_if(false)/log(false)/warn, reset_sink_ext",
                 "only the warning reaches the C sink (level WARN, APPLICATION, text ends with the message); unknown level -> INVALID_ARGUMENT; nothing after reset",
                 sinkResult == CNA_RESULT_SUCCESS && readBack == CNA_LOG_LEVEL_WARN && beforeWarn == 0 && afterWarn.lines == 1 && afterWarn.level == CNA_LOG_LEVEL_WARN &&
                     afterWarn.category == CNA_LOG_CATEGORY_APPLICATION && EndsWith(afterWarn.text, "museum C probe: one warning") && badLevel == CNA_RESULT_INVALID_ARGUMENT &&
                     resetResult == CNA_RESULT_SUCCESS && capture.lines == 1);
}

static void PlatformChecks(CProbe* p)
{
    CNA_Platform platform = 99;
    CNA_Bool apple = CNA_TRUE, mobile = CNA_TRUE;
    CNA_DesktopOS os = 99;
    char name[64];
    uint64_t sized = 0, bytes = 0;
    cna_platform_get_current(&platform);
    cna_platform_get_is_apple_ext(&apple);
    cna_platform_get_is_mobile_ext(&mobile);
    cna_desktop_os_get_current(&os);
    cna_platform_get_current_name_size_ext(&sized);
    const CNA_Result copied = cna_platform_copy_current_name_ext(name, sizeof name - 1, &bytes);
    cprobe_terminate(name, sizeof name, copied, &bytes);
    cprobe_got(p, "platform %u, apple %u, mobile %u, desktop OS %u, name \"%s\" (%llu bytes)", (unsigned)platform, apple, mobile, (unsigned)os, name, (unsigned long long)sized);
#if defined(__linux__)
    CPROBE_CHECK(p, "cna_platform_get_current, is_apple/is_mobile_ext, desktop_os_get_current, platform name", "DESKTOP(0), no, no, LINUX(1), \"Linux\" (5 bytes)",
                 platform == CNA_PLATFORM_DESKTOP && apple == CNA_FALSE && mobile == CNA_FALSE && os == CNA_DESKTOP_OS_LINUX && strcmp(name, "Linux") == 0 && sized == 5u);
#else
    CPROBE_CHECK(p, "cna_platform_get_current, is_apple/is_mobile_ext, desktop_os_get_current, platform name", "DESKTOP(0) and a name whose size matches the copy",
                 platform == CNA_PLATFORM_DESKTOP && sized == bytes && bytes > 0u);
#endif
}

static void RendererChecks(CProbe* p)
{
    CNA_GraphicsRendererCategory gles = 99, software = 99, fna = 99, headless = 99;
    cna_graphics_renderer_get_category(CNA_GRAPHICS_RENDERER_OPENGLES3, &gles);
    cna_graphics_renderer_get_category(CNA_GRAPHICS_RENDERER_SOFTWARE, &software);
    cna_graphics_renderer_get_category(CNA_GRAPHICS_RENDERER_FNA3D, &fna);
    cna_graphics_renderer_get_category(CNA_GRAPHICS_RENDERER_HEADLESS, &headless);
    char categoryName[64];
    uint64_t categoryBytes = 0, categorySize = 0;
    cna_graphics_renderer_category_get_name_size(CNA_GRAPHICS_RENDERER_CATEGORY_TRANSLATION_LAYER, &categorySize);
    const CNA_Result categoryCopied = cna_graphics_renderer_category_copy_name(CNA_GRAPHICS_RENDERER_CATEGORY_TRANSLATION_LAYER, categoryName, sizeof categoryName - 1, &categoryBytes);
    cprobe_terminate(categoryName, sizeof categoryName, categoryCopied, &categoryBytes);
    CNA_GraphicsRendererCategory unknown = 77;
    const CNA_Result unknownResult = cna_graphics_renderer_get_category((CNA_GraphicsRendererType)999u, &unknown);
    cprobe_got(p, "OPENGLES3 %u, SOFTWARE %u, FNA3D %u, HEADLESS %u; \"%s\" (%llu); identity 999 -> %u, output %s", (unsigned)gles, (unsigned)software, (unsigned)fna,
               (unsigned)headless, categoryName, (unsigned long long)categorySize, (unsigned)unknownResult, unknown == 77 ? "untouched" : "written");
    CPROBE_CHECK(p, "cna_graphics_renderer_get_category, category_get_name_size/copy_name", "NATIVE(0), SOFTWARE(2), TRANSLATION_LAYER(1), DIAGNOSTIC(4); \"TranslationLayer\" (16); 999 -> INVALID_ARGUMENT",
                 gles == CNA_GRAPHICS_RENDERER_CATEGORY_NATIVE && software == CNA_GRAPHICS_RENDERER_CATEGORY_SOFTWARE && fna == CNA_GRAPHICS_RENDERER_CATEGORY_TRANSLATION_LAYER &&
                     headless == CNA_GRAPHICS_RENDERER_CATEGORY_DIAGNOSTIC && strcmp(categoryName, "TranslationLayer") == 0 && categorySize == 16u &&
                     unknownResult == CNA_RESULT_INVALID_ARGUMENT && unknown == 77);

    /* Identities, names and the compiled-in set (pure queries; the latched selection is checked in the C game probe, whose own game creates a device). */
    CNA_Bool available = CNA_FALSE, recognized = CNA_FALSE, bogus = CNA_TRUE;
    CNA_GraphicsRendererType parsed = 0, compiled = 0, parsedCompiled = 0, ignored = 0;
    cna_graphics_renderer_try_parse_name_ext(cprobe_view("opengles3"), &parsed, &recognized);
    const CNA_Result bogusResult = cna_graphics_renderer_try_parse_name_ext(cprobe_view("NoSuchRenderer"), &ignored, &bogus);
    char compiledName[64];
    uint64_t compiledBytes = 0, compiledSize = 0;
    cna_graphics_renderer_get_current_name_size(&compiledSize);
    const CNA_Result nameCopied = cna_graphics_renderer_copy_current_name(compiledName, sizeof compiledName - 1, &compiledBytes);
    cprobe_terminate(compiledName, sizeof compiledName, nameCopied, &compiledBytes);
    cna_graphics_renderer_get_current_type(&compiled);
    cna_graphics_renderer_get_is_available_ext(compiled, &available);
    CNA_Bool compiledRecognized = CNA_FALSE;
    cna_graphics_renderer_try_parse_name_ext(cprobe_view(compiledName), &parsedCompiled, &compiledRecognized);
    CNA_GraphicsRendererType list[64];
    uint64_t listed = 0, counted = 0;
    cna_graphics_renderer_get_available_count_ext(&counted);
    const CNA_Result listResult = cna_graphics_renderer_copy_available_ext(list, 64u, &listed);
    int compiledListed = 0;
    for (uint64_t i = 0; listResult == CNA_RESULT_SUCCESS && i < listed && i < 64u; ++i) compiledListed = compiledListed || list[i] == compiled;
    CNA_GraphicsRendererMaturity maturity = 99;
    cna_graphics_renderer_get_maturity(CNA_GRAPHICS_RENDERER_STUB, &maturity);
    char maturityName[32];
    uint64_t maturityBytes = 0;
    const CNA_Result maturityCopied = cna_graphics_renderer_maturity_copy_name(CNA_GRAPHICS_RENDERER_MATURITY_PRODUCTION, maturityName, sizeof maturityName - 1, &maturityBytes);
    cprobe_terminate(maturityName, sizeof maturityName, maturityCopied, &maturityBytes);
    cprobe_got(p, "\"opengles3\" -> %u/%u, \"NoSuchRenderer\" -> %u/%u; compiled \"%s\" (%llu) = %u, parsed %u, available %u, listed %d of %llu/%llu; STUB maturity %u; \"%s\"", recognized,
               (unsigned)parsed, (unsigned)bogusResult, bogus, compiledName, (unsigned long long)compiledSize, (unsigned)compiled, (unsigned)parsedCompiled, available, compiledListed,
               (unsigned long long)listed, (unsigned long long)counted, (unsigned)maturity, maturityName);
    CPROBE_CHECK(p, "cna_graphics_renderer_try_parse_name_ext, get_current_name_size/copy_current_name, get_current_type, get_is_available/get_available_count/copy_available_ext, get_maturity, maturity_copy_name",
                 "\"opengles3\" -> 3 (case-insensitive), an unknown name -> SUCCESS/not recognized; the compiled name parses to get_current_type, which is available and listed; \"Production\"",
                 recognized == CNA_TRUE && parsed == CNA_GRAPHICS_RENDERER_OPENGLES3 && bogusResult == CNA_RESULT_SUCCESS && bogus == CNA_FALSE && compiledRecognized == CNA_TRUE &&
                     parsedCompiled == compiled && compiledSize == compiledBytes && available == CNA_TRUE && compiledListed && counted == listed && maturity <= CNA_GRAPHICS_RENDERER_MATURITY_DEPRECATED &&
                     strcmp(maturityName, "Production") == 0);
}

static void TitleAndExceptionChecks(CProbe* p)
{
    char saved[256], readBack[256];
    uint64_t savedBytes = 0, readBytes = 0;
    const CNA_Result savedResult = cna_assembly_copy_title_ext(saved, sizeof saved - 1, &savedBytes);
    cprobe_terminate(saved, sizeof saved, savedResult, &savedBytes);
    const CNA_Result setResult = cna_assembly_set_title_ext(cprobe_view("CNA from C"));
    const CNA_Result readResult = cna_assembly_copy_title_ext(readBack, sizeof readBack - 1, &readBytes);
    cprobe_terminate(readBack, sizeof readBack, readResult, &readBytes);
    cna_assembly_set_title_ext(cprobe_view(saved));
    char restored[256];
    uint64_t restoredBytes = 0;
    cprobe_terminate(restored, sizeof restored, cna_assembly_copy_title_ext(restored, sizeof restored - 1, &restoredBytes), &restoredBytes);
    cprobe_got(p, "set %u, read back \"%s\" (%llu bytes), restored \"%s\"", (unsigned)setResult, readBack, (unsigned long long)readBytes, restored);
    CPROBE_CHECK(p, "cna_assembly_set_title_ext / copy_title_ext", "\"CNA from C\" (10 bytes), then the previous title back",
                 setResult == CNA_RESULT_SUCCESS && strcmp(readBack, "CNA from C") == 0 && readBytes == 10u && strcmp(restored, saved) == 0);
    /* The title is documented as process-wide, and this process declared one at static initialisation through CNA's C++ API
       (Main.cpp: AssemblyTitleAttributeEXT "CNA 3D Museum"). The C library must see that one state, not a copy of its own. */
    cprobe_got(p, "the C ABI's title before this check: \"%s\" (%llu bytes)", saved, (unsigned long long)savedBytes);
    CPROBE_CHECK(p, "one process, one CNA: cna_assembly_copy_title_ext sees the title the host declared through CNA's C++ API",
                 "\"CNA 3D Museum\" (the title is process-wide; libcna_c_api.so must not carry a second copy of CNA's state)", savedResult == CNA_RESULT_SUCCESS && strcmp(saved, "CNA 3D Museum") == 0);

    /* A failure of the C layer's own argument check was not a canonical exception: no exception type, no parameter name. */
    CNA_Vector3 v = {1, 2, 3};
    const CNA_Result nullOut = cna_vector3_length(v, NULL);
    uint64_t messageSize = 0, typeSize = 99, paramSize = 99;
    cna_error_get_last_message_size(&messageSize);
    cna_error_get_last_exception_type_size_ext(&typeSize);
    cna_error_get_last_exception_param_name_size_ext(&paramSize);
    char typeName[8];
    uint64_t typeBytes = 99;
    const CNA_Result typeCopy = cna_error_copy_last_exception_type_ext(typeName, sizeof typeName, &typeBytes);
    char paramName[8];
    uint64_t paramBytes = 99;
    const CNA_Result paramCopy = cna_error_copy_last_exception_param_name_ext(paramName, sizeof paramName, &paramBytes);
    cprobe_got(p, "result %u, message %llu bytes, exception type %llu bytes (copy %u/%llu), parameter name %llu bytes (copy %u/%llu)", (unsigned)nullOut, (unsigned long long)messageSize,
               (unsigned long long)typeSize, (unsigned)typeCopy, (unsigned long long)typeBytes, (unsigned long long)paramSize, (unsigned)paramCopy, (unsigned long long)paramBytes);
    CPROBE_CHECK(p, "cna_error_get_last_message_size and the exception type/parameter name ext after a C-layer NULL-output check",
                 "INVALID_ARGUMENT with a message; exception type and parameter name both 0 bytes (not a canonical exception)",
                 nullOut == CNA_RESULT_INVALID_ARGUMENT && messageSize > 0u && typeSize == 0u && paramSize == 0u && typeCopy == CNA_RESULT_SUCCESS && typeBytes == 0u &&
                     paramCopy == CNA_RESULT_SUCCESS && paramBytes == 0u);
}

void museum_c_probe_core(CProbe* probe)
{
    LoggerChecks(probe);
    PlatformChecks(probe);
    RendererChecks(probe);
    TitleAndExceptionChecks(probe);
}
