#!/bin/bash
set -e

# Check if clang++ is available
if ! command -v clang++ &> /dev/null; then
    echo "clang++ not found. Please install clang++ with MinGW-w64 target support."
    echo "On Ubuntu/Debian: sudo apt-get install clang mingw-w64"
    exit 1
fi

# Check if the SDK submodule is populated
if [ ! -d "StarRupture SDK" ] || [ -z "$(ls -A 'StarRupture SDK')" ]; then
    echo "StarRupture SDK submodule not found or empty."
    echo "Run: git submodule update --init --recursive"
    exit 1
fi

# Generate AssertionNoops.h
echo "Generating AssertionNoops.h..."
python3 << 'EOF'
import re
import os

sdk_source = "StarRupture SDK/Client/SDK"
output_file = "include/AssertionNoops.h"

os.makedirs("include", exist_ok=True)

assertions = []
for root, dirs, files in os.walk(sdk_source):
    for file in files:
        if file.endswith(".h") or file.endswith(".hpp") or file.endswith(".inl"):
            filepath = os.path.join(root, file)
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                matches = re.findall(r'(check|checkf|ensure|ensuref|verify|verifyf)\s*\(', content)
                for match in matches:
                    if match not in assertions:
                        assertions.append(match)

with open(output_file, 'w') as f:
    f.write("#pragma once\n\n")
    for assertion in sorted(set(assertions)):
        f.write(f"#define {assertion}(x, ...)\n")

print(f"Generated {output_file} with {len(set(assertions))} assertion macros")
EOF

# Create Windows.h shim for case sensitivity
echo "Creating Windows.h shim..."
mkdir -p include
cat > include/Windows.h << 'EOF'
#ifndef WINDOWS_H_SHIM
#define WINDOWS_H_SHIM
#include <windows.h>
#endif
EOF

# Compile the plugin
echo "Building PingMod.dll..."
mkdir -p "bin/x64/Client Release/plugins"

clang++ -std=c++20 -fms-extensions -fms-compatibility-version=19.29 \
    -I"StarRupture SDK/Client" \
    -I"StarRupture SDK/Client/SDK" \
    -Iinclude \
    -DWIN32_LEAN_AND_MEAN \
    -D_WINDOWS \
    -DMODLOADER_CLIENT_BUILD \
    -O2 -s \
    "StarRupture SDK/Client/SDK/Basic.cpp" \
    "StarRupture SDK/Client/SDK/CoreUObject_functions.cpp" \
    "StarRupture SDK/Client/SDK/Engine_functions.cpp" \
    "StarRupture SDK/Client/SDK/Chimera_functions.cpp" \
    PingMod/dllmain.cpp \
    PingMod/plugin.cpp \
    -shared -o "bin/x64/Client Release/plugins/PingMod.dll" \
    -luser32 -lkernel32 \
    -static-libgcc -static-libstdc++

echo "Build complete! Output: bin/x64/Client Release/plugins/PingMod.dll"
