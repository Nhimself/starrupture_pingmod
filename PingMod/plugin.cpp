#include "plugin.h"

// Game SDK - only Basic.hpp needed: provides FindClassByName / GetDefaultObjectImpl
#include "Basic.hpp"

static IPluginLogger* g_logger  = nullptr;
static IPluginHooks*  g_hooks   = nullptr;

// UCrPlayerPingDeveloperSettings memory layout (from game SDK dump):
//   0x0038  float  DisappearTime
//   0x003C  float  TraceLength    <-- the 100m distance limit
//
// Unreal Engine uses centimetres; 100 metres = 10,000 cm.
// We raise it to 1,000,000 cm (10 km) — effectively unlimited for normal play.
static constexpr float UNLIMITED_TRACE_LENGTH = 1'000'000.0f;

static void ApplyPingDistancePatch()
{
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
	float original = *traceLength;
	*traceLength = UNLIMITED_TRACE_LENGTH;

	if (g_logger)
		g_logger->Info("PingMod", "TraceLength patched: %.0f cm (%.0f m) -> %.0f cm (%.0f m)",
			original, original / 100.0f,
			UNLIMITED_TRACE_LENGTH, UNLIMITED_TRACE_LENGTH / 100.0f);
}

static void OnAnyWorldBeginPlay(SDK::UWorld* /*world*/, const char* /*worldName*/)
{
	// Re-apply on every world load to handle map transitions
	ApplyPingDistancePatch();
}

static PluginInfo s_plugin_info = {
	"PingMod",
	"1.0.0",
	"Nhimself",
	"Removes the 100-meter distance limit on pings, allowing players to set pings at any range.",
	PLUGIN_INTERFACE_VERSION
};

PluginInfo* GetPluginInfo()
{
	return &s_plugin_info;
}

bool PluginInit(IPluginLogger* logger, IPluginConfig* /*config*/, IPluginScanner* /*scanner*/, IPluginHooks* hooks)
{
	g_logger = logger;
	g_hooks  = hooks;

	// Apply immediately — CDO is available as soon as the engine starts
	ApplyPingDistancePatch();

	// Re-apply on each world load to survive map transitions
	hooks->World->RegisterOnAnyWorldBeginPlay(OnAnyWorldBeginPlay);

	logger->Info("PingMod", "Initialized — ping distance limit removed");
	return true;
}

void PluginShutdown()
{
	if (g_hooks)
		g_hooks->World->UnregisterOnAnyWorldBeginPlay(OnAnyWorldBeginPlay);

	g_logger = nullptr;
	g_hooks  = nullptr;
}
