#pragma once

#include "plugin_interface.h"

extern "C"
{
	__declspec(dllexport) SPluginInfo* __cdecl GetPluginInfo();
	__declspec(dllexport) void __cdecl PluginInit(IPluginHooks* Hooks);
	__declspec(dllexport) void __cdecl PluginShutdown();
}
