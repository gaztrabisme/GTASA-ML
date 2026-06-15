# Bridge plugin (C++ ASI) — M0 hello-bridge

This is the game-side half of the bridge: a GTA SA ASI plugin (built on the **Plugin SDK**)
that opens a TCP server and streams a per-frame **heartbeat** to the Python client.

> **Heads-up:** none of this can be built or run on Linux/CI — it needs **Windows + Visual
> Studio 2022 + a v1.0 US copy of GTA San Andreas**. The source here is written to be correct
> against the verified Plugin SDK API, but the first real compile happens on your machine.
> Expect to iterate on the build setup together — send me the first compiler/linker error.

## Files

| File | Role |
| --- | --- |
| `src/Main.cpp` | Plugin entry; registers frame events; sends heartbeat each frame. |
| `src/HeartbeatServer.h` | Header-only non-blocking Winsock TCP server. |
| `CMakeLists.txt` | Optional CMake build (32-bit). The VS-template path below is lower-risk. |

## Prerequisites (one-time)

1. **GTA SA v1.0 US ("HOODLUM")** install. If you have the Steam/RGL version, **downgrade to
   v1.0** first (see the context doc + community downgrade guides). This is the single most
   common time-sink — get it right before anything else.
2. **Ultimate ASI Loader** (x86) installed into the game folder (drop the proxy DLL, e.g.
   `dinput8.dll`). Confirm it loads ASIs before continuing.
3. **Visual Studio 2022** with the "Desktop development with C++" workload.
4. **Plugin SDK** (DK22Pac) cloned and set up per its wiki
   ("Installing development environment (IDE)"). Note the env var it expects
   (e.g. `PLUGIN_SDK_DIR`) and **avoid spaces in the path**.

## Recommended build path: the SDK's VS template (lowest risk)

The Plugin SDK ships a "Creating a new plugin" project template. Use it — it wires up the
include/lib paths, the 32-bit target, and the `.asi` output for you.

1. Create a new **SA** plugin project from the SDK template.
2. **Add** `src/Main.cpp` and `src/HeartbeatServer.h` to the project (remove the template's
   stub source).
3. Ensure the configuration is **x86 / Win32** (SA is 32-bit) and **Release**.
4. `Ws2_32.lib` is already linked via `#pragma comment(lib, "Ws2_32.lib")` in the header, so
   no extra linker setup is needed for the socket code.
5. Build → you get a `.asi` (a renamed DLL).

## Alternative: CMake

`CMakeLists.txt` is provided as a starting point. You must point it at the SDK and confirm the
SDK's library name/layout (these vary by how you built the SDK). Configure for **Win32**:

```bat
cmake -B build -A Win32 -DPLUGIN_SDK_DIR=C:/path/to/plugin-sdk
cmake --build build --config Release
```

If the SDK linkage fights you, fall back to the VS-template path above — it's the supported route.

## Install & run (M0 acceptance test)

1. Copy the built `GTASAML.asi` into the game folder (or its `scripts/` / `plugins/` folder,
   wherever your ASI loader scans).
2. Start the Python client **first or after** (it retries):
   ```bat
   python ..\scripts\m0_heartbeat_client.py
   ```
3. Launch GTA SA and load into gameplay.
4. **Pass condition:** the client prints a steady stream of heartbeats with an increasing
   `frame` and a `~N msg/s` rate. That `msg/s` is your first real throughput number — note it,
   it sets expectations for the RL step rate.

## Troubleshooting

- **No connection ever:** plugin didn't load (wrong game version / ASI loader not active /
  built as x64 instead of Win32), or port 7777 is taken.
- **Connects then nothing:** frame hook not firing — confirm you're actually in gameplay, not
  a menu/loading screen.
- **Crash on load:** almost always a game-version / SDK-address mismatch — re-check v1.0 US.

## Config

Port is `kPort` in `src/Main.cpp` (default `7777`) and `--port` on the Python client.
