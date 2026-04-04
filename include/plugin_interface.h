#pragma once

#include <windows.h>
#include <cstdint>

// Plugin interface version history - increment MAX when new hooks/features are added.
// Increment MIN only when an ABI-breaking change is unavoidable.
// Loader accepts any plugin whose interfaceVersion is in [MIN, MAX].
// Plugins compiled against older (but still supported) headers load without recompilation
// because all interface structs are append-only — new fields are always added at the end.
// v2: Added RegisterEngineShutdownCallback / UnregisterEngineShutdownCallback to IPluginHooks
// v3: Replaced std::vector return types in IPluginScanner with caller-buffer API to fix
//     cross-DLL heap corruption (EXCEPTION_ACCESS_VIOLATION on plugin load)
// v4: Added FindXrefsToAddress / FindXrefsToAddressInModule / FindXrefsToAddressInMainModule
//     to IPluginScanner for function-pointer cross-reference scanning
// v5: Added EngineAlloc / EngineFree / IsEngineAllocatorAvailable to IPluginHooks
//     for safe FString / engine-owned memory manipulation from plugins
// v6: Added RegisterAnyWorldBeginPlayCallback / UnregisterAnyWorldBeginPlayCallback to IPluginHooks
//     for receiving notifications when ANY world begins play (not just ChimeraMain)
// v7: Added RegisterSaveLoadedCallback / UnregisterSaveLoadedCallback to IPluginHooks
//     for receiving notifications when UCrMassSaveSubsystem::OnSaveLoaded fires (save fully loaded)
// v8: Added RegisterExperienceLoadCompleteCallback / UnregisterExperienceLoadCompleteCallback
//     to IPluginHooks for receiving notifications when UCrExperienceManagerComponent::OnExperienceLoadComplete fires
// v9: Added RegisterEngineTickCallback / UnregisterEngineTickCallback to IPluginHooks
//     for receiving per-frame game-thread tick notifications (UGameEngine::Tick)
// v10: Added RegisterActorBeginPlayCallback / UnregisterActorBeginPlayCallback
//      to IPluginHooks for receiving notifications when any AActor::BeginPlay fires
// v11: Added RegisterActorDestroyedCallback / UnregisterActorDestroyedCallback to IPluginHooks
//      for receiving notifications when any AActor::Destroyed fires
// v12: Added RegisterPlayerJoinedCallback / UnregisterPlayerJoinedCallback to IPluginHooks
//      for receiving notifications when a player joins the game
// v13: Added RegisterPlayerLeftCallback / UnregisterPlayerLeftCallback to IPluginHooks
//      for receiving notifications when a player leaves the game
// v14: Added RegisterInputActionCallback / UnregisterInputActionCallback to IPluginHooks (CLIENT ONLY)
//      for receiving notifications when an input action occurs
//      Added RegisterUICustomPanelCallback / UnregisterUICustomPanelCallback to IPluginHooks (CLIENT ONLY)
//      for custom UI panel creation and deletion
// v15: Added RegisterHudPostRenderCallback / UnregisterHudPostRenderCallback to IPluginHooks (CLIENT ONLY)
//      for post-render HUD drawing operations
// v16: Added RegisterModKeyPressedCallback / UnregisterModKeyPressedCallback to IPluginHooks (CLIENT ONLY)
//      for keybind subscriptions

// Forward declarations
struct IPluginLogger;
struct IPluginConfig;
struct IPluginScanner;
struct IPluginHooks;
struct ImGuiContext;
struct ImDrawList;
struct ImVec2;
struct ImVec4;

// Maximum plugin name length
#define MAX_PLUGIN_NAME_LENGTH 256

// Function pointer types
typedef void(__cdecl* FSpawnerHookFn)(void* spawnerAddress);
typedef void(__cdecl* FOnPreSpawnEnemyCallback)(void* spawnerAddress, void* spawnDataAddress);
typedef void(__cdecl* FOnPostSpawnEnemyCallback)(void* spawnerAddress, void* spawnDataAddress, void* enemyInstanceAddress);
typedef void(__cdecl* FEngineShutdownCallback)();
typedef void(__cdecl* FEngineTickCallback)(float DeltaTime);
typedef void(__cdecl* FAnyWorldBeginPlayCallback)(void* WorldAddress);
typedef void(__cdecl* FSaveLoadedCallback)(void* SaveSubsystemAddress);
typedef void(__cdecl* FExperienceLoadCompleteCallback)(void* ExperienceManagerAddress);
typedef void(__cdecl* FActorBeginPlayCallback)(void* ActorAddress);
typedef void(__cdecl* FActorDestroyedCallback)(void* ActorAddress);
typedef void(__cdecl* FPlayerJoinedCallback)(void* PlayerControllerAddress);
typedef void(__cdecl* FPlayerLeftCallback)(void* PlayerControllerAddress);
typedef void(__cdecl* FInputActionCallback)(const char* ActionName, bool bPressed);
typedef void(__cdecl* FUICustomPanelCallback)(bool bOpening);
typedef void(__cdecl* FHudPostRenderCallback)();
typedef void(__cdecl* FModKeyPressedCallback)(uint32_t KeyCode);

// Enums
enum class EModKey : uint32_t
{
	MK_F1 = 0x70,
	MK_F2 = 0x71,
	MK_F3 = 0x72,
	MK_F4 = 0x73,
	MK_F5 = 0x74,
	MK_F6 = 0x75,
	MK_F7 = 0x76,
	MK_F8 = 0x77,
	MK_F9 = 0x78,
	MK_F10 = 0x79,
	MK_F11 = 0x7A,
	MK_F12 = 0x7B,
	MK_A = 0x41,
	MK_B = 0x42,
	MK_C = 0x43,
	MK_D = 0x44,
	MK_E = 0x45,
	MK_F = 0x46,
	MK_G = 0x47,
	MK_H = 0x48,
	MK_I = 0x49,
	MK_J = 0x4A,
	MK_K = 0x4B,
	MK_L = 0x4C,
	MK_M = 0x4D,
	MK_N = 0x4E,
	MK_O = 0x4F,
	MK_P = 0x50,
	MK_Q = 0x51,
	MK_R = 0x52,
	MK_S = 0x53,
	MK_T = 0x54,
	MK_U = 0x55,
	MK_V = 0x56,
	MK_W = 0x57,
	MK_X = 0x58,
	MK_Y = 0x59,
	MK_Z = 0x5A,
	MK_0 = 0x30,
	MK_1 = 0x31,
	MK_2 = 0x32,
	MK_3 = 0x33,
	MK_4 = 0x34,
	MK_5 = 0x35,
	MK_6 = 0x36,
	MK_7 = 0x37,
	MK_8 = 0x38,
	MK_9 = 0x39,
	MK_NUM0 = 0x60,
	MK_NUM1 = 0x61,
	MK_NUM2 = 0x62,
	MK_NUM3 = 0x63,
	MK_NUM4 = 0x64,
	MK_NUM5 = 0x65,
	MK_NUM6 = 0x66,
	MK_NUM7 = 0x67,
	MK_NUM8 = 0x68,
	MK_NUM9 = 0x69,
	MK_MOUSE_LEFT = 0x01,
	MK_MOUSE_RIGHT = 0x02,
	MK_MOUSE_MIDDLE = 0x04,
	MK_SHIFT = 0x10,
	MK_CTRL = 0x11,
	MK_ALT = 0x12,
};

// IPluginLogger - provides logging functionality
struct IPluginLogger
{
	virtual void Trace(const char* Message) = 0;
	virtual void Debug(const char* Message) = 0;
	virtual void Info(const char* Message) = 0;
	virtual void Warning(const char* Message) = 0;
	virtual void Error(const char* Message) = 0;
	virtual ~IPluginLogger() = default;
};

// IPluginConfig - configuration management
struct IPluginConfig
{
	// Get the value of a configuration entry (returns nullptr if not found)
	virtual const char* GetConfigValue(const char* Key) = 0;
	// Set the value of a configuration entry
	virtual void SetConfigValue(const char* Key, const char* Value) = 0;
	// Get integer value (with default fallback)
	virtual int32_t GetConfigInt(const char* Key, int32_t DefaultValue = 0) = 0;
	// Set integer value
	virtual void SetConfigInt(const char* Key, int32_t Value) = 0;
	// Get float value (with default fallback)
	virtual float GetConfigFloat(const char* Key, float DefaultValue = 0.0f) = 0;
	// Set float value
	virtual void SetConfigFloat(const char* Key, float Value) = 0;
	// Get boolean value (with default fallback)
	virtual bool GetConfigBool(const char* Key, bool DefaultValue = false) = 0;
	// Set boolean value
	virtual void SetConfigBool(const char* Key, bool Value) = 0;
	virtual ~IPluginConfig() = default;
};

// IPluginScanner - pattern scanning and cross-reference detection
struct IPluginScanner
{
	// Scan for a pattern in the executable (IDA-style pattern matching)
	// Returns the address if found, nullptr otherwise
	virtual void* FindPattern(const char* Pattern) = 0;

	// Scan for a pattern in the executable and get all matches
	// Caller provides output buffer and size
	// Returns the number of matches found
	virtual size_t FindPatternAll(const char* Pattern, void** OutAddresses, size_t MaxAddresses) = 0;

	// Get all cross-references TO a specific address
	// Caller provides output buffer and size
	// Returns the number of references found
	virtual size_t FindXrefsToAddress(void* Address, void** OutXrefs, size_t MaxXrefs) = 0;

	// Get all cross-references TO a specific address within a specific module
	// Caller provides output buffer and size
	// Returns the number of references found
	virtual size_t FindXrefsToAddressInModule(void* Address, const char* ModuleName, void** OutXrefs, size_t MaxXrefs) = 0;

	// Get all cross-references TO a specific address within the main executable
	// Caller provides output buffer and size
	// Returns the number of references found
	virtual size_t FindXrefsToAddressInMainModule(void* Address, void** OutXrefs, size_t MaxXrefs) = 0;

	virtual ~IPluginScanner() = default;
};

// ImGui wrapper function table - access ImGui functions through this
// instead of directly linking to ImGui
struct IModLoaderImGui
{
	// Get the current ImGui context
	ImGuiContext* (*GetContext)();

	// ImGui functions - only the commonly needed ones are exposed
	void (*ShowDemoWindow)(bool* p_open);
	void (*Begin)(const char* name, bool* p_open, uint32_t flags);
	void (*End)();
	bool (*Button)(const char* label, const struct ImVec2 size);
	void (*Text)(const char* fmt, ...);
	bool (*Checkbox)(const char* label, bool* v);
	bool (*InputFloat)(const char* label, float* v, float step, float step_fast, const char* format, uint32_t flags);
	bool (*InputInt)(const char* label, int32_t* v, int32_t step, int32_t step_fast, uint32_t flags);
	bool (*InputText)(const char* label, char* buf, size_t buf_size, uint32_t flags, int (*callback)(ImGuiInputTextCallbackData* data));
	bool (*SliderFloat)(const char* label, float* v, float v_min, float v_max, const char* format, uint32_t flags);
	bool (*SliderInt)(const char* label, int32_t* v, int32_t v_min, int32_t v_max, const char* format, uint32_t flags);
	bool (*ColorEdit4)(const char* label, float col[4], uint32_t flags);
	void (*Spacing)();
	void (*Separator)();
	void (*SameLine)(float offset_from_start, float spacing);
	void (*Indent)(float indent_w);
	void (*Unindent)(float indent_w);
	bool (*CollapsingHeader)(const char* label, uint32_t flags);
	void (*SetNextWindowSize)(const struct ImVec2 size, uint32_t cond);
	void (*SetNextWindowPos)(const struct ImVec2 pos, uint32_t cond, const struct ImVec2 pivot);
	bool (*IsMouseHoveringRect)(const struct ImVec2 r_min, const struct ImVec2 r_max, bool clip);
	struct ImDrawList* (*GetWindowDrawList)();
	void (*GetWindowPos)(struct ImVec2* out_pos);
	void (*GetWindowSize)(struct ImVec2* out_size);
	void (*DrawList_AddLine)(struct ImDrawList* draw_list, const struct ImVec2 a, const struct ImVec2 b, uint32_t col, float thickness);
	void (*DrawList_AddRect)(struct ImDrawList* draw_list, const struct ImVec2 p_min, const struct ImVec2 p_max, uint32_t col, float rounding, uint32_t flags, float thickness);
	void (*DrawList_AddRectFilled)(struct ImDrawList* draw_list, const struct ImVec2 p_min, const struct ImVec2 p_max, uint32_t col, float rounding, uint32_t flags);
	void (*DrawList_AddCircle)(struct ImDrawList* draw_list, const struct ImVec2 center, float radius, uint32_t col, int num_segments, float thickness);
	void (*DrawList_AddCircleFilled)(struct ImDrawList* draw_list, const struct ImVec2 center, float radius, uint32_t col, int num_segments);
	void (*DrawList_AddText)(struct ImDrawList* draw_list, const struct ImVec2 pos, uint32_t col, const char* text);
	void (*PushStyleColor)(uint32_t idx, const struct ImVec4 col);
	void (*PopStyleColor)(int count);
	void (*PushStyleVar)(uint32_t idx, float val);
	void (*PopStyleVar)(int count);
};

// IPluginHooks - main plugin interface
struct IPluginHooks
{
	// === Spawner Hooks ===
	virtual uint64_t RegisterSpawnerHook(FSpawnerHookFn Callback) = 0;
	virtual void UnregisterSpawnerHook(uint64_t CallbackID) = 0;

	virtual uint64_t RegisterOnPreSpawnEnemyCallback(FOnPreSpawnEnemyCallback Callback) = 0;
	virtual void UnregisterOnPreSpawnEnemyCallback(uint64_t CallbackID) = 0;

	virtual uint64_t RegisterOnPostSpawnEnemyCallback(FOnPostSpawnEnemyCallback Callback) = 0;
	virtual void UnregisterOnPostSpawnEnemyCallback(uint64_t CallbackID) = 0;

	// === Engine Hooks ===
	virtual uint64_t RegisterEngineShutdownCallback(FEngineShutdownCallback Callback) = 0;
	virtual void UnregisterEngineShutdownCallback(uint64_t CallbackID) = 0;

	virtual uint64_t RegisterEngineTickCallback(FEngineTickCallback Callback) = 0;
	virtual void UnregisterEngineTickCallback(uint64_t CallbackID) = 0;

	// === World Hooks ===
	virtual uint64_t RegisterAnyWorldBeginPlayCallback(FAnyWorldBeginPlayCallback Callback) = 0;
	virtual void UnregisterAnyWorldBeginPlayCallback(uint64_t CallbackID) = 0;

	virtual uint64_t RegisterSaveLoadedCallback(FSaveLoadedCallback Callback) = 0;
	virtual void UnregisterSaveLoadedCallback(uint64_t CallbackID) = 0;

	virtual uint64_t RegisterExperienceLoadCompleteCallback(FExperienceLoadCompleteCallback Callback) = 0;
	virtual void UnregisterExperienceLoadCompleteCallback(uint64_t CallbackID) = 0;

	// === Actor Hooks ===
	virtual uint64_t RegisterActorBeginPlayCallback(FActorBeginPlayCallback Callback) = 0;
	virtual void UnregisterActorBeginPlayCallback(uint64_t CallbackID) = 0;

	virtual uint64_t RegisterActorDestroyedCallback(FActorDestroyedCallback Callback) = 0;
	virtual void UnregisterActorDestroyedCallback(uint64_t CallbackID) = 0;

	// === Player Hooks ===
	virtual uint64_t RegisterPlayerJoinedCallback(FPlayerJoinedCallback Callback) = 0;
	virtual void UnregisterPlayerJoinedCallback(uint64_t CallbackID) = 0;

	virtual uint64_t RegisterPlayerLeftCallback(FPlayerLeftCallback Callback) = 0;
	virtual void UnregisterPlayerLeftCallback(uint64_t CallbackID) = 0;

	// === Client-Only: Input Hooks ===
	virtual uint64_t RegisterInputActionCallback(FInputActionCallback Callback) = 0;
	virtual void UnregisterInputActionCallback(uint64_t CallbackID) = 0;

	// === Client-Only: Custom UI Panels ===
	virtual uint64_t RegisterUICustomPanelCallback(FUICustomPanelCallback Callback) = 0;
	virtual void UnregisterUICustomPanelCallback(uint64_t CallbackID) = 0;

	// === Client-Only: HUD Post-Render ===
	virtual uint64_t RegisterHudPostRenderCallback(FHudPostRenderCallback Callback) = 0;
	virtual void UnregisterHudPostRenderCallback(uint64_t CallbackID) = 0;

	// === Client-Only: Keybind Subscriptions ===
	virtual uint64_t RegisterModKeyPressedCallback(EModKey Key, FModKeyPressedCallback Callback) = 0;
	virtual void UnregisterModKeyPressedCallback(uint64_t CallbackID) = 0;

	// === Low-Level Hooks (Advanced) ===
	// Install a low-level hook at the specified address
	virtual bool RegisterLowLevelHook(void* Address, void* HookFunction, void** OutOriginal) = 0;
	// Uninstall a low-level hook
	virtual bool UnregisterLowLevelHook(void* Address, void* OriginalFunction) = 0;

	// === Memory Operations ===
	// Patch memory at the specified address
	virtual bool PatchMemory(void* Address, const void* Data, size_t Size) = 0;
	// Allocate memory that will be freed by the engine
	virtual void* EngineAlloc(size_t Size) = 0;
	// Free memory allocated by EngineAlloc
	virtual void EngineFree(void* Ptr) = 0;
	// Check if engine allocator is available
	virtual bool IsEngineAllocatorAvailable() = 0;

	// Get the logger interface
	virtual IPluginLogger* GetLogger() = 0;
	// Get the config interface
	virtual IPluginConfig* GetConfig() = 0;
	// Get the scanner interface
	virtual IPluginScanner* GetScanner() = 0;
	// Get the ImGui interface
	virtual IModLoaderImGui* GetImGui() = 0;

	virtual ~IPluginHooks() = default;
};

// Plugin information structure
struct SPluginInfo
{
	// Plugin name
	char Name[MAX_PLUGIN_NAME_LENGTH];
	// Plugin version
	uint32_t Version;
	// Interface version (must be between MIN and MAX)
	uint32_t InterfaceVersion;
};

// Plugin interface versions
#define PLUGIN_INTERFACE_VERSION_MIN 14
#define PLUGIN_INTERFACE_VERSION_MAX 16

// Exported plugin functions
extern "C"
{
	// Get plugin information - must be exported by the plugin DLL
	__declspec(dllexport) SPluginInfo* __cdecl GetPluginInfo();

	// Initialize plugin - must be exported by the plugin DLL
	__declspec(dllexport) void __cdecl PluginInit(IPluginHooks* Hooks);

	// Shutdown plugin - must be exported by the plugin DLL
	__declspec(dllexport) void __cdecl PluginShutdown();
}
