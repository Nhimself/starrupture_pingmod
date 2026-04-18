#include "plugin.h"

// Game SDK - only Basic.hpp needed: provides FindClassByName / GetDefaultObjectImpl
#include "Basic.hpp"

static IPluginLogger* g_logger  = nullptr;
static IPluginConfig* g_config  = nullptr;
static IPluginHooks*  g_hooks   = nullptr;

// UCrPlayerPingDeveloperSettings memory layout (from game SDK dump):
//   0x0038  float  DisappearTime
//   0x003C  float  TraceLength    <-- the distance limit (default ~100 m = 10,000 cm)
//
// Unreal Engine uses centimetres internally.

// Config defaults
static constexpr float DEFAULT_MAX_PING_DISTANCE_M = 1000.0f; // metres
static constexpr float MIN_PING_DISTANCE_M         = 50.0f;
static constexpr float MAX_PING_DISTANCE_M         = 2000.0f; // above this risks hitting skybox/backdrop

static constexpr ConfigEntry CONFIG_ENTRIES[] =
{
    {
        /* section      */ "General",
        /* key          */ "Enabled",
        /* type         */ ConfigValueType::Boolean,
        /* defaultValue */ "true",
        /* description  */ "Enable or disable PingMod. When false the game's original 100 m limit applies.",
        /* rangeMin     */ 0.0f,
        /* rangeMax     */ 0.0f
    },
    {
        /* section      */ "General",
        /* key          */ "MaxPingDistanceM",
        /* type         */ ConfigValueType::Float,
        /* defaultValue */ "1000.0",
        /* description  */ "Maximum ping distance in metres (default: 1000).\n"
                           "The original game limit is 100 m.\n"
                           "Values above ~2000 m risk hitting out-of-bounds geometry\n"
                           "(skybox / backdrop) and may cause server crashes.",
        /* rangeMin     */ MIN_PING_DISTANCE_M,
        /* rangeMax     */ MAX_PING_DISTANCE_M
    }
};

static constexpr ConfigSchema CONFIG_SCHEMA =
{
    CONFIG_ENTRIES,
    2
};

static float ReadConfiguredTraceLength()
{
    float metres = g_config->ReadFloat("PingMod", "General", "MaxPingDistanceM",
                                       DEFAULT_MAX_PING_DISTANCE_M);

    // Clamp to safe range so a bad config value can't cause crashes
    if (metres < MIN_PING_DISTANCE_M) metres = MIN_PING_DISTANCE_M;
    if (metres > MAX_PING_DISTANCE_M) metres = MAX_PING_DISTANCE_M;

    return metres * 100.0f; // metres -> centimetres (Unreal units)
}

static bool IsEnabled()
{
    return g_config->ReadBool("PingMod", "General", "Enabled", true);
}

static void ApplyPingDistancePatch()
{
    if (!IsEnabled())
    {
        if (g_logger) g_logger->Info("PingMod", "Disabled via config — skipping patch");
        return;
    }

    using namespace SDK;

    UClass* pingSettingsClass = BasicFilesImplUtils::FindClassByName("CrPlayerPingDeveloperSettings");
    if (!pingSettingsClass)
    {
        if (g_logger) g_logger->Warn("PingMod", "CrPlayerPingDeveloperSettings class not found");
        return;
    }

    UObject* cdo = BasicFilesImplUtils::GetDefaultObjectImpl(pingSettingsClass);
    if (!cdo)
    {
        if (g_logger) g_logger->Warn("PingMod", "CrPlayerPingDeveloperSettings CDO not found");
        return;
    }

    // TraceLength is at offset 0x003C in the object
    float* traceLength = reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(cdo) + 0x003C);
    float original     = *traceLength;
    float target       = ReadConfiguredTraceLength();
    *traceLength       = target;

    if (g_logger)
        g_logger->Info("PingMod", "TraceLength patched: %.0f cm (%.0f m) -> %.0f cm (%.0f m)",
            original, original / 100.0f,
            target,   target   / 100.0f);
}

static void OnAnyWorldBeginPlay(SDK::UWorld* /*world*/, const char* /*worldName*/)
{
    // Re-apply on every world load to handle map transitions
    ApplyPingDistancePatch();
}

static PluginInfo s_plugin_info = {
    "PingMod",
    "1.1.0",
    "Nhimself",
    "Raises the ping distance limit beyond the default 100 m. "
    "Can be enabled/disabled and configured via MaxPingDistanceM in the mod config (default: 1000 m, max: 2000 m).",
    PLUGIN_INTERFACE_VERSION
};

PluginInfo* GetPluginInfo()
{
    return &s_plugin_info;
}

bool PluginInit(IPluginSelf* self)
{
    g_logger = self->logger;
    g_config = self->config;
    g_hooks  = self->hooks;

    self->config->InitializeFromSchema("PingMod", &CONFIG_SCHEMA);

    // Apply immediately — CDO is available as soon as the engine starts
    ApplyPingDistancePatch();

    // Re-apply on each world load to survive map transitions
    self->hooks->World->RegisterOnAnyWorldBeginPlay(OnAnyWorldBeginPlay);

    self->logger->Info("PingMod", "Initialized (Enabled=%s, MaxPingDistanceM=%.0f)",
        IsEnabled() ? "true" : "false",
        ReadConfiguredTraceLength() / 100.0f);
    return true;
}

void PluginShutdown()
{
    if (g_hooks)
        g_hooks->World->UnregisterOnAnyWorldBeginPlay(OnAnyWorldBeginPlay);

    g_logger = nullptr;
    g_config = nullptr;
    g_hooks  = nullptr;
}
