# PingMod

PingMod is a [StarRupture Mod Loader](https://github.com/AlienXAXS/StarRupture-ModLoader) plugin that removes the default 100-metre distance limit on player pings, allowing you to mark targets at any range up to a configurable maximum.

## How It Works

The game's ping system fires a raycast of length `TraceLength` from the player when you middle-click. If nothing is hit within that distance the ping is silently discarded — hence the 100 m wall. PingMod patches the `TraceLength` value in `UCrPlayerPingDeveloperSettings` at runtime, raising it to your configured maximum.

## Installation

1. Install the [StarRupture Mod Loader](https://github.com/AlienXAXS/StarRupture-ModLoader) by placing `dwmapi.dll` in your game's `Win64` binaries folder.
2. Download `PingMod.dll` from the [latest release](../../releases/latest).
3. Place `PingMod.dll` in the `Plugins` subdirectory next to `dwmapi.dll`.

## Configuration

On first launch the mod creates an INI file at:

```
%LOCALAPPDATA%\StarRupturePlugins\PingMod\PingMod.ini
```

| Key | Default | Range | Description |
|-----|---------|-------|-------------|
| `MaxPingDistanceM` | `500.0` | 50 – 2000 | Maximum ping distance in metres. Values outside this range are clamped automatically. |

> **Note:** Values above ~2000 m risk hitting out-of-bounds skybox geometry and may cause server instability. The mod enforces a hard cap of 2000 m regardless of the configured value.

Example config:

```ini
[General]
MaxPingDistanceM=500.0
```

## Building from Source

**Requirements:** Visual Studio 2022 with the C++ workload, or `clang++` with MinGW-w64 for Linux cross-compilation.

```bash
git clone --recurse-submodules https://github.com/Nhimself/starrupture_pingmod.git
```

**Windows (Visual Studio):**

Open `StarRupture-PingMod.sln` and build the `Client Release | x64` configuration. The output is placed in `bin\x64\Client Release\plugins\PingMod.dll`.

**Linux (cross-compile):**

```bash
./build.sh
```

Output: `bin/x64/Client Release/plugins/PingMod.dll`
