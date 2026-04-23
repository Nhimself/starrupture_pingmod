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
// v11: Added RegisterPlayerJoinedCallback / UnregisterPlayerJoinedCallback
//      to IPluginHooks for receiving notifications when ACrGameModeBase::PostLogin fires
//      (player controller fully connected and ready on server)
// v12: Added RegisterPlayerLeftCallback / UnregisterPlayerLeftCallback
//      to IPluginHooks for receiving notifications when ACrGameModeBase::Logout fires
//      (player controller about to be destroyed — still valid at callback time)
// v14: Replaced flat function-pointer fields (v1-v12) with typed sub-interface pointers.
//      IPluginHooks now contains only 7 sub-interface pointers — all functionality
//      accessed via hooks->Group->Method(...). MIN bumped to 14 (ABI break).
//      Added 10 named callback typedefs (PluginEngineInitCallback, etc.).
//      Sub-interfaces:
//        Spawner  — Before/After hooks for ActivateSpawner, DeactivateSpawner, DoSpawning
//                   (Before callbacks return bool; true cancels + suppresses After callbacks)
//        Hooks    — low-level hook install/remove/query  (hooks->Hooks->Install)
//        Memory   — patch/nop/read/alloc utilities       (hooks->Memory->Patch)
//        Engine   — init/shutdown/tick subscriptions     (hooks->Engine->RegisterOnInit)
//        World    — world-begin-play/save/experience     (hooks->World->RegisterOnWorldBeginPlay)
//        Players  — player joined/left subscriptions     (hooks->Players->RegisterOnPlayerJoined)
//        Actors   — actor begin-play subscriptions       (hooks->Actors->RegisterOnActorBeginPlay)
// v15: Added EModKey enum, EModKeyEvent enum, PluginKeybindCallback typedef,
//      IPluginInputEvents sub-interface, and Input pointer in IPluginHooks.
//      Input sub-interface supports registration by EModKey enum or by UE key name string.
//      Input is non-null on client builds only; always nullptr on server/generic builds.
//        Input    — keybind event subscriptions          (hooks->Input->RegisterKeybind)
//      Also added (folded into v15 before first release):
//      IModLoaderImGui function table, PluginImGuiRenderCallback, PluginPanelDesc, PanelHandle,
//      PluginConfigChangedCallback, IPluginUIEvents sub-interface, and UI pointer in IPluginHooks.
//      UI is non-null on client builds only; always nullptr on server/generic builds.
//        UI       — custom panel registration + config-change callbacks (hooks->UI->RegisterPanel)
//      RegisterPanel now returns a PanelHandle (opaque pointer stable for the plugin's lifetime).
//      SetPanelOpen / SetPanelClose both take PanelHandle instead of a title string, so a plugin
//      can only open/close its own panels (it cannot affect panels owned by other plugins).
// v16: Added PluginWidgetDesc, WidgetHandle, RegisterWidget, UnregisterWidget, and SetWidgetVisible
//      to IPluginUIEvents.  Widgets are always-on ImGui windows rendered every frame regardless of
//      whether the modloader window is open.  Plugin authors can call SetWidgetVisible to hide or
//      show a widget (e.g. in response to a keybind or config option).
//        UI       — always-on widget registration           (hooks->UI->RegisterWidget)
//      Also added (folded into v16 before first release):
//      PluginHUDPostRenderCallback, IPluginHUDEvents sub-interface, and HUD pointer in IPluginHooks.
//      HUD is non-null on client builds only; always nullptr on server/generic builds.
//        HUD      — AHUD::PostRender callbacks + GatherPlayersData address (hooks->HUD->RegisterOnPostRender)
//      Extended IPluginEngineEvents with GetStaticLoadObjectAddress() on all build types.
//      Patterns for AHUD_PostRender, StaticLoadObject, and GatherPlayersData moved from
//      compass_patterns.h into the modloader's scan_patterns.h and owned by the modloader.
//      hooks->HUD and hooks->UI and hooks->Input are all nullptr on server — always null-check.
#define PLUGIN_INTERFACE_VERSION_MIN 22  // oldest plugin ABI still accepted by this loader
#define PLUGIN_INTERFACE_VERSION_MAX 23  // current interface version (this header)
#define PLUGIN_INTERFACE_VERSION PLUGIN_INTERFACE_VERSION_MAX  // alias used by plugins in PluginInfo

// Log levels
enum class PluginLogLevel
{
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4
};

// Config value types
enum class ConfigValueType
{
    String,
    Integer,
    Float,
    Boolean
};

// Config entry definition for auto-generation
struct ConfigEntry
{
    const char* section;      // INI section name (e.g., "General", "Advanced")
    const char* key;          // INI key name
    ConfigValueType type;     // Value type
    const char* defaultValue; // Default value as string (converted based on type)
    const char* description;  // Optional description/comment
    float rangeMin;           // Slider min -- ignored when rangeMax <= rangeMin
    float rangeMax;           // Slider max -- when rangeMax > rangeMin a slider is shown
};

// Config schema - defines all config entries for a plugin
struct ConfigSchema
{
    const ConfigEntry* entries; // Array of config entries
    int entryCount;          // Number of entries in array
};

// Universal logger interface provided by mod loader
struct IPluginLogger
{
    // Log a message with the specified level
    void (*Log)(PluginLogLevel level, const char* pluginName, const char* message);

    // Convenience methods
    void (*Trace)(const char* pluginName, const char* format, ...);
    void (*Debug)(const char* pluginName, const char* format, ...);
    void (*Info)(const char* pluginName, const char* format, ...);
    void (*Warn)(const char* pluginName, const char* format, ...);
    void (*Error)(const char* pluginName, const char* format, ...);
};

// Config manager interface provided by mod loader
struct IPluginConfig
{
    // Read string value from plugin's config file
    bool (*ReadString)(const char* pluginName, const char* section, const char* key, char* outValue, int maxLen, const char* defaultValue);

    // Write string value to plugin's config file
    bool (*WriteString)(const char* pluginName, const char* section, const char* key, const char* value);

    // Read integer value from plugin's config file
    int (*ReadInt)(const char* pluginName, const char* section, const char* key, int defaultValue);

    // Write integer value to plugin's config file
    bool (*WriteInt)(const char* pluginName, const char* section, const char* key, int value);

    // Read float value from plugin's config file
    float (*ReadFloat)(const char* pluginName, const char* section, const char* key, float defaultValue);

    // Write float value to plugin's config file
    bool (*WriteFloat)(const char* pluginName, const char* section, const char* key, float value);

    // Read boolean value from plugin's config file
    bool (*ReadBool)(const char* pluginName, const char* section, const char* key, bool defaultValue);

    // Write boolean value to plugin's config file
    bool (*WriteBool)(const char* pluginName, const char* section, const char* key, bool value);

    // Initialize plugin config from schema
    bool (*InitializeFromSchema)(const char* pluginName, const ConfigSchema* schema);

    // Validate and repair config file based on schema
    void (*ValidateConfig)(const char* pluginName, const ConfigSchema* schema);
};

// A single cross-reference result returned by the xref scanner.
struct PluginXRef
{
    uintptr_t address;    // Address of the referencing instruction / pointer slot
    bool      isRelative; // true = relative CALL/JMP  |  false = absolute pointer
};

// Pattern scanner interface provided by mod loader
struct IPluginScanner
{
    uintptr_t (*FindPatternInMainModule)(const char* pattern);
    uintptr_t (*FindPatternInModule)(HMODULE module, const char* pattern);
    int (*FindAllPatternsInMainModule)(const char* pattern, uintptr_t* outAddresses, int maxResults);
    int (*FindAllPatternsInModule)(HMODULE module, const char* pattern, uintptr_t* outAddresses, int maxResults);
    uintptr_t (*FindUniquePattern)(const char** patterns, int patternCount, int* outPatternIndex);
    int (*FindXrefsToAddress)(uintptr_t targetAddress, uintptr_t start, size_t size,
                              PluginXRef* outXRefs, int maxResults);
    int (*FindXrefsToAddressInModule)(uintptr_t targetAddress, HMODULE module,
                                      PluginXRef* outXRefs, int maxResults);
    int (*FindXrefsToAddressInMainModule)(uintptr_t targetAddress,
                                          PluginXRef* outXRefs, int maxResults);
};

// Opaque hook handle
typedef void* HookHandle;

// Forward declare SDK::UWorld for callback
namespace SDK { class UWorld; }

// ============================================================
// Event callback typedefs (v14)
// ============================================================
typedef void (*PluginEngineInitCallback)();
typedef void (*PluginEngineShutdownCallback)();
typedef void (*PluginEngineTickCallback)(float deltaSeconds);
typedef void (*PluginWorldBeginPlayCallback)(SDK::UWorld* world);
typedef void (*PluginAnyWorldBeginPlayCallback)(SDK::UWorld* world, const char* worldName);
typedef void (*PluginSaveLoadedCallback)();
typedef void (*PluginExperienceLoadCompleteCallback)();
typedef void (*PluginActorBeginPlayCallback)(void* actor);
typedef void (*PluginPlayerJoinedCallback)(void* playerController);
typedef void (*PluginPlayerLeftCallback)(void* exitingController);
typedef void (*PluginHUDPostRenderCallback)(void* hud);
typedef void (*PluginWorldEndPlayCallback)(SDK::UWorld* world, const char* worldName);
typedef void (*PluginNetworkMessageCallback)(const char* pluginName, const char* typeTag,
                                             const uint8_t* data, size_t size);
typedef void (*PluginNetworkServerMessageCallback)(void* senderPlayerController,
                                                   const char* pluginName,
                                                   const char* typeTag,
                                                   const uint8_t* data, size_t size);

// ============================================================
// Spawner hook callback typedefs (v14)
// ============================================================
typedef bool (*PluginBeforeActivateSpawnerCallback)(void* spawner, bool bDisableAggroLock);
typedef void (*PluginAfterActivateSpawnerCallback)(void* spawner, bool bDisableAggroLock);
typedef bool (*PluginBeforeDeactivateSpawnerCallback)(void* spawner, bool bPermanently);
typedef void (*PluginAfterDeactivateSpawnerCallback)(void* spawner, bool bPermanently);
typedef bool (*PluginBeforeDoSpawningCallback)(void* spawner);
typedef void (*PluginAfterDoSpawningCallback)(void* spawner);

// ============================================================
// IPluginHookUtils — low-level hook install/remove/query (v14)
// ============================================================
struct IPluginHookUtils
{
    HookHandle (*Install)(uintptr_t targetAddress, void* detourFunction, void** originalFunction);
    void       (*Remove)(HookHandle handle);
    bool       (*IsInstalled)(HookHandle handle);
};

// ============================================================
// IPluginMemoryUtils — memory patch/nop/read/alloc utilities (v14)
// ============================================================
struct IPluginMemoryUtils
{
    bool  (*Patch)(uintptr_t address, const uint8_t* data, size_t size);
    bool  (*Nop)(uintptr_t address, size_t size);
    bool  (*Read)(uintptr_t address, void* buffer, size_t size);
    void* (*Alloc)(size_t count, uint32_t alignment);
    void  (*Free)(void* ptr);
    bool  (*IsAllocatorAvailable)();
};

// ============================================================
// IPluginEngineEvents (v14)
// ============================================================
struct IPluginEngineEvents
{
    void (*RegisterOnInit)(PluginEngineInitCallback);
    void (*UnregisterOnInit)(PluginEngineInitCallback);
    void (*RegisterOnShutdown)(PluginEngineShutdownCallback);
    void (*UnregisterOnShutdown)(PluginEngineShutdownCallback);
    void (*RegisterOnTick)(PluginEngineTickCallback);
    void (*UnregisterOnTick)(PluginEngineTickCallback);
    uintptr_t (*GetStaticLoadObjectAddress)();
};

// ============================================================
// IPluginWorldEvents (v14)
// ============================================================
struct IPluginWorldEvents
{
    void (*RegisterOnWorldBeginPlay)(PluginWorldBeginPlayCallback);
    void (*UnregisterOnWorldBeginPlay)(PluginWorldBeginPlayCallback);
    void (*RegisterOnAnyWorldBeginPlay)(PluginAnyWorldBeginPlayCallback);
    void (*UnregisterOnAnyWorldBeginPlay)(PluginAnyWorldBeginPlayCallback);
    void (*RegisterOnSaveLoaded)(PluginSaveLoadedCallback);
    void (*UnregisterOnSaveLoaded)(PluginSaveLoadedCallback);
    void (*RegisterOnExperienceLoadComplete)(PluginExperienceLoadCompleteCallback);
    void (*UnregisterOnExperienceLoadComplete)(PluginExperienceLoadCompleteCallback);
    void (*RegisterOnBeforeWorldEndPlay)(PluginWorldEndPlayCallback);    // v20
    void (*UnregisterOnBeforeWorldEndPlay)(PluginWorldEndPlayCallback);  // v20
    void (*RegisterOnAfterWorldEndPlay)(PluginWorldEndPlayCallback);     // v20
    void (*UnregisterOnAfterWorldEndPlay)(PluginWorldEndPlayCallback);   // v20
};

// ============================================================
// IPluginPlayerEvents (v14)
// ============================================================
struct IPluginPlayerEvents
{
    void (*RegisterOnPlayerJoined)(PluginPlayerJoinedCallback);
    void (*UnregisterOnPlayerJoined)(PluginPlayerJoinedCallback);
    void (*RegisterOnPlayerLeft)(PluginPlayerLeftCallback);
    void (*UnregisterOnPlayerLeft)(PluginPlayerLeftCallback);
};

// ============================================================
// IPluginActorEvents (v14)
// ============================================================
struct IPluginActorEvents
{
    void (*RegisterOnActorBeginPlay)(PluginActorBeginPlayCallback);
    void (*UnregisterOnActorBeginPlay)(PluginActorBeginPlayCallback);
};

// ============================================================
// IPluginSpawnerHooks (v14)
// ============================================================
struct IPluginSpawnerHooks
{
    void (*RegisterOnBeforeActivate)(PluginBeforeActivateSpawnerCallback callback);
    void (*UnregisterOnBeforeActivate)(PluginBeforeActivateSpawnerCallback callback);
    void (*RegisterOnAfterActivate)(PluginAfterActivateSpawnerCallback callback);
    void (*UnregisterOnAfterActivate)(PluginAfterActivateSpawnerCallback callback);
    void (*RegisterOnBeforeDeactivate)(PluginBeforeDeactivateSpawnerCallback callback);
    void (*UnregisterOnBeforeDeactivate)(PluginBeforeDeactivateSpawnerCallback callback);
    void (*RegisterOnAfterDeactivate)(PluginAfterDeactivateSpawnerCallback callback);
    void (*UnregisterOnAfterDeactivate)(PluginAfterDeactivateSpawnerCallback callback);
    void (*RegisterOnBeforeDoSpawning)(PluginBeforeDoSpawningCallback callback);
    void (*UnregisterOnBeforeDoSpawning)(PluginBeforeDoSpawningCallback callback);
    void (*RegisterOnAfterDoSpawning)(PluginAfterDoSpawningCallback callback);
    void (*UnregisterOnAfterDoSpawning)(PluginAfterDoSpawningCallback callback);
};

// ============================================================
// IPluginNetworkChannel (v17)
// ============================================================
struct IPluginNetworkChannel
{
    bool (*IsServer)();
    void (*SendPacketToClient)(void* playerController, const struct IPluginSelf* self,
                               const char* typeTag, const uint8_t* data, size_t size);
    void (*SendPacketToAllClients)(const struct IPluginSelf* self, const char* typeTag,
                                   const uint8_t* data, size_t size);
    void (*RegisterMessageHandler)(const struct IPluginSelf* self, const char* typeTag,
                                   PluginNetworkMessageCallback callback);
    void (*UnregisterMessageHandler)(const struct IPluginSelf* self, const char* typeTag,
                                     PluginNetworkMessageCallback callback);
    void (*SendPacketToServer)(const struct IPluginSelf* self, const char* typeTag,
                               const uint8_t* data, size_t size);                          // v18
    void (*RegisterServerMessageHandler)(const struct IPluginSelf* self, const char* typeTag,
                                         PluginNetworkServerMessageCallback callback);     // v18
    void (*UnregisterServerMessageHandler)(const struct IPluginSelf* self, const char* typeTag,
                                           PluginNetworkServerMessageCallback callback);   // v18
    void (*ExcludeFromBroadcast)(void* playerController);    // v18
    void (*UnexcludeFromBroadcast)(void* playerController);  // v18
};

// ============================================================
// EModKey (v15)
// ============================================================
enum class EModKey : uint32_t
{
    F1 = 0, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Zero, One, Two, Three, Four, Five, Six, Seven, Eight, Nine,
    Escape, Tab, CapsLock, SpaceBar, Enter, BackSpace, Delete, Insert,
    LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt,
    Up, Down, Left, Right, Home, End, PageUp, PageDown,
    Tilde, Hyphen, Equals, LeftBracket, RightBracket, Backslash,
    Semicolon, Apostrophe, Comma, Period, Slash,
    NumPadZero, NumPadOne, NumPadTwo, NumPadThree, NumPadFour,
    NumPadFive, NumPadSix, NumPadSeven, NumPadEight, NumPadNine,
    Add, Subtract, Multiply, Divide, Decimal,
    LeftMouseButton, RightMouseButton, MiddleMouseButton,
    ThumbMouseButton, ThumbMouseButton2,
    Unknown
};

enum class EModKeyEvent : uint32_t
{
    Pressed  = 0,
    Released = 1
};

typedef void (*PluginKeybindCallback)(EModKey key, EModKeyEvent event);

// ============================================================
// IPluginInputEvents (v15, client only)
// ============================================================
struct IPluginInputEvents
{
    void (*RegisterKeybind)(EModKey key, EModKeyEvent event, PluginKeybindCallback callback);
    void (*UnregisterKeybind)(EModKey key, EModKeyEvent event, PluginKeybindCallback callback);
    void (*RegisterKeybindByName)(const char* keyName, EModKeyEvent event, PluginKeybindCallback callback);
    void (*UnregisterKeybindByName)(const char* keyName, EModKeyEvent event, PluginKeybindCallback callback);
};

// ============================================================
// IModLoaderImGui (v15)
// ============================================================
struct IModLoaderImGui
{
    void (*Text)(const char* text);
    void (*TextColored)(float r, float g, float b, float a, const char* text);
    void (*TextDisabled)(const char* text);
    void (*TextWrapped)(const char* text);
    void (*LabelText)(const char* label, const char* text);
    void (*SeparatorText)(const char* label);
    bool (*InputText)(const char* label, char* buf, size_t buf_size);
    bool (*InputInt)(const char* label, int* v, int step, int step_fast);
    bool (*InputFloat)(const char* label, float* v, float step, float step_fast, const char* format);
    bool (*Checkbox)(const char* label, bool* v);
    bool (*SliderFloat)(const char* label, float* v, float v_min, float v_max, const char* format);
    bool (*SliderInt)(const char* label, int* v, int v_min, int v_max, const char* format);
    bool (*Button)(const char* label);
    bool (*SmallButton)(const char* label);
    void (*SameLine)(float offset_from_start_x, float spacing);
    void (*NewLine)();
    void (*Separator)();
    void (*Spacing)();
    void (*Indent)(float indent_w);
    void (*Unindent)(float indent_w);
    void (*PushIDStr)(const char* str_id);
    void (*PushIDInt)(int int_id);
    void (*PopID)();
    bool (*BeginCombo)(const char* label, const char* preview_value);
    bool (*Selectable)(const char* label, bool selected);
    void (*EndCombo)();
    bool (*CollapsingHeader)(const char* label);
    bool (*TreeNodeStr)(const char* label);
    void (*TreePop)();
    bool (*ColorEdit3)(const char* label, float col[3]);
    bool (*ColorEdit4)(const char* label, float col[4]);
    void (*SetTooltip)(const char* text);
    bool (*IsItemHovered)();
    void (*SetNextItemWidth)(float item_width);
};

typedef void (*PluginImGuiRenderCallback)(IModLoaderImGui* imgui);

struct PluginPanelDesc
{
    const char* buttonLabel;
    const char* windowTitle;
    PluginImGuiRenderCallback renderFn;
};

typedef void* PanelHandle;
typedef void* WidgetHandle;

struct PluginWidgetDesc
{
    const char* name;
    PluginImGuiRenderCallback renderFn;
};

typedef void (*PluginConfigChangedCallback)(const char* section, const char* key, const char* newValue);

// ============================================================
// IPluginUIEvents (v15/v16, client only)
// ============================================================
struct IPluginUIEvents
{
    PanelHandle (*RegisterPanel)(const PluginPanelDesc* desc);
    void (*UnregisterPanel)(PanelHandle handle);
    void (*RegisterOnConfigChanged)(PluginConfigChangedCallback callback);
    void (*UnregisterOnConfigChanged)(PluginConfigChangedCallback callback);
    void (*SetPanelOpen)(PanelHandle handle);
    void (*SetPanelClose)(PanelHandle handle);
    WidgetHandle (*RegisterWidget)(const PluginWidgetDesc* desc);
    void (*UnregisterWidget)(WidgetHandle handle);
    void (*SetWidgetVisible)(WidgetHandle handle, bool visible);
};

// ============================================================
// IPluginHUDEvents (v16, client only)
// ============================================================
struct IPluginHUDEvents
{
    void      (*RegisterOnPostRender)(PluginHUDPostRenderCallback callback);
    void      (*UnregisterOnPostRender)(PluginHUDPostRenderCallback callback);
    uintptr_t (*GetGatherPlayersDataAddress)();
};

// ============================================================
// IPluginNativePointers (v21)
// ============================================================
struct IPluginNativePointers
{
    uintptr_t (*EngineLoopInit)();
    uintptr_t (*GameEngineInit)();
    uintptr_t (*EngineLoopExit)();
    uintptr_t (*EnginePreExit)();
    uintptr_t (*EngineTick)();
    uintptr_t (*WorldBeginPlay)();
    uintptr_t (*WorldEndPlay)();
    uintptr_t (*SaveLoaded)();
    uintptr_t (*ExperienceLoadComplete)();
    uintptr_t (*ActorBeginPlay)();
    uintptr_t (*PlayerJoined)();
    uintptr_t (*PlayerLeft)();
    uintptr_t (*SpawnerActivate)();
    uintptr_t (*SpawnerDeactivate)();
    uintptr_t (*SpawnerDoSpawning)();
    uintptr_t (*HUDPostRender)();       // client only (nullptr on server/generic)
    uintptr_t (*ClientMessageExec)();   // client only (nullptr on server/generic)
};

// ============================================================
// IPluginHttpServer (v22) — server builds only
// ============================================================
enum class HttpMethod : uint32_t
{
    Get, Post, Put, Delete, Patch, Options, Head, Other
};

enum class HttpRequestAction : uint32_t
{
    Approve,
    Deny
};

struct PluginHttpRequest
{
    const char* url;
    const uint8_t* body;
    size_t bodyLen;
    HttpMethod method;
};

struct PluginHttpResponse
{
    int statusCode;
    const char* contentType;
    const uint8_t* body;
    size_t bodyLen;
};

typedef HttpRequestAction (*PluginHttpRequestFilterCallback)(const PluginHttpRequest* req);
typedef void              (*PluginHttpRouteCallback)(const PluginHttpRequest* req,
                                                     PluginHttpResponse* resp);

struct IPluginHttpServer
{
    bool (*AddRoute)(const struct IPluginSelf* self, const char* folderName);
    void (*RemoveRoute)(const struct IPluginSelf* self, const char* folderName);
    void (*RegisterOnRawRequest)(PluginHttpRequestFilterCallback callback);
    void (*UnregisterOnRawRequest)(PluginHttpRequestFilterCallback callback);
    bool (*AddRawRoute)(const struct IPluginSelf* self, const char* urlPrefix,
                        PluginHttpRouteCallback callback);
    void (*RemoveRawRoute)(const struct IPluginSelf* self, const char* urlPrefix);
};

// ============================================================
// IPluginHooks — top-level hook interface (v14+)
// ============================================================
struct IPluginHooks
{
    IPluginSpawnerHooks*    Spawner;         // v14
    IPluginHookUtils*       Hooks;           // v14
    IPluginMemoryUtils*     Memory;          // v14
    IPluginEngineEvents*    Engine;          // v14
    IPluginWorldEvents*     World;           // v14
    IPluginPlayerEvents*    Players;         // v14
    IPluginActorEvents*     Actors;          // v14
    IPluginInputEvents*     Input;           // v15 (client only, nullptr on server)
    IPluginUIEvents*        UI;              // v15 (client only, nullptr on server)
    IPluginHUDEvents*       HUD;             // v16 (client only, nullptr on server)
    IPluginNetworkChannel*  Network;         // v17
    IPluginNativePointers*  NativePointers;  // v21
    IPluginHttpServer*      HttpServer;      // v22 (server only, nullptr on client/generic)
};

// Plugin metadata structure
struct PluginInfo
{
    const char* name;
    const char* version;
    const char* author;
    const char* description;
    int interfaceVersion;
};

// ============================================================
// IPluginSelf (v19) — plugin identity + subsystem access
// Passed as the sole argument to PluginInit.
// ============================================================
struct IPluginSelf
{
    const char*     name;
    const char*     version;
    IPluginLogger*  logger;
    IPluginConfig*  config;
    IPluginScanner* scanner;
    IPluginHooks*   hooks;
};

// Function pointer types for the plugin loader
typedef PluginInfo* (*GetPluginInfoFunc)();
typedef bool (*PluginInitFunc)(IPluginSelf* self);
typedef void (*PluginShutdownFunc)();

#define PLUGIN_GET_INFO_FUNC_NAME "GetPluginInfo"
#define PLUGIN_INIT_FUNC_NAME "PluginInit"
#define PLUGIN_SHUTDOWN_FUNC_NAME "PluginShutdown"
