#include "plugin.h"

// Game SDK - only include what we need to modify UCrPlayerPingDeveloperSettings
#include "Basic.hpp"

static IPluginHooks* g_hooks = nullptr;
static IPluginLogger* g_logger = nullptr;

static uint64_t g_worldBeginPlayId = 0;

// UCrPlayerPingDeveloperSettings memory layout (from game SDK dump):
//   0x0038  float  DisappearTime
//   0x003C  float  TraceLength    <-- the 100m distance limit
//
// Unreal Engine uses centimetres; 100 metres = 10,000 cm.
// We replace it with 1,000,000 cm (10 km) — effectively unlimited for normal play.
static constexpr float UNLIMITED_TRACE_LENGTH = 1'000'000.0f;

static void ApplyPingDistancePatch()
{
	using namespace SDK;

	UClass* pingSettingsClass = BasicFilesImplUtils::FindClassByName("CrPlayerPingDeveloperSettings");
	if (!pingSettingsClass)
	{
		if (g_logger) g_logger->Warning("[PingMod] CrPlayerPingDeveloperSettings class not found");
		return;
	}

	UObject* cdo = BasicFilesImplUtils::GetDefaultObjectImpl(pingSettingsClass);
	if (!cdo)
	{
		if (g_logger) g_logger->Warning("[PingMod] CrPlayerPingDeveloperSettings CDO not found");
		return;
	}

	// TraceLength is at offset 0x003C in the object
	float* traceLength = reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(cdo) + 0x003C);
	float original = *traceLength;
	*traceLength = UNLIMITED_TRACE_LENGTH;

	if (g_logger)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "[PingMod] TraceLength patched: %.0f -> %.0f cm (%.0f m -> %.0f m)",
			original, UNLIMITED_TRACE_LENGTH,
			original / 100.0f, UNLIMITED_TRACE_LENGTH / 100.0f);
		g_logger->Info(buf);
	}
}

static void OnAnyWorldBeginPlay(void* /*WorldAddress*/)
{
	// Re-apply on every world load to handle map transitions
	ApplyPingDistancePatch();
}

static SPluginInfo s_plugin_info = {
	"PingMod",
	1,
	PLUGIN_INTERFACE_VERSION_MAX
};

SPluginInfo* GetPluginInfo()
{
	return &s_plugin_info;
}

void PluginInit(IPluginHooks* Hooks)
{
	g_hooks = Hooks;
	g_logger = Hooks->GetLogger();

	// Apply immediately in case the engine is already running with a world
	ApplyPingDistancePatch();

	// Also re-apply on each world begin play to survive map transitions
	g_worldBeginPlayId = Hooks->RegisterAnyWorldBeginPlayCallback(OnAnyWorldBeginPlay);

	if (g_logger) g_logger->Info("[PingMod] Initialized — ping distance limit removed");
}

void PluginShutdown()
{
	if (g_hooks && g_worldBeginPlayId)
		g_hooks->UnregisterAnyWorldBeginPlayCallback(g_worldBeginPlayId);

	g_hooks = nullptr;
	g_logger = nullptr;
	g_worldBeginPlayId = 0;
}
