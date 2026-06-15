# 01 — Decision Record: Development Environment

**Date:** 2026-06-15
**Status:** Decided
**Decision:** Develop and run everything on **native Windows**.

## Context

The user has a machine that can dual-boot Ubuntu or Windows, and a GPU. GTA: San Andreas is a
32-bit Windows game. The project's highest-risk component is the **in-process ASI plugin** (native
code that builds against the game's MSVC-compiled ABI and is injected into the running game).

## Options considered

1. **Windows (native)** — chosen.
2. **Windows + WSL2** — Windows for game/plugin, WSL for the Python/ML shell.
3. **Ubuntu via Proton/Wine** — game under Wine, plugin cross-compiled on Linux.

## Decision & rationale

**Windows**, because it removes risk from exactly the component most likely to derail the project:

- The entire GTA modding ecosystem (Plugin SDK, Ultimate ASI Loader, CLEO, downgrade guides) is
  **Windows + Visual Studio 2022** first-class. ASI plugins build and load natively.
- The ML stack (PyTorch/CUDA, Stable-Baselines3) runs fine on Windows with the GPU.
- External memory reads still work natively (`pymem`/`ReadProcessMemory`) for the eventual
  read-external / act-in-process hybrid.
- Claude Code runs on Windows too, so direct build/launch/test by the assistant is preserved —
  this was the user's original reason for considering Linux, and it is not Linux-exclusive.

## Why NOT Linux/Proton (verified, not assumed)

A verification pass (see sources) found that a Linux build is *possible* but adds avoidable risk
to the hardest component:

- `plugin-sdk` documents a **MinGW-w64** path and a **clang-cl + xwin** (MSVC-ABI) path exists.
- **But** MinGW/GCC `__thiscall` matches MSVC only for scalar/pointer cases; **by-value
  struct/class returns** (`CVector`, `CMatrix`, etc., which the SDK uses heavily) are a
  silent-corruption failure mode. The ABI-safe route (clang-cl+xwin) requires a hand-written
  CMake build, and **no one has been found to have built a plugin-sdk `.asi` end-to-end on Linux**.
- Native Windows/MSVC eliminates this entire class of bug by construction.

Conclusion: the only thing Linux won was general dev ergonomics — not worth taking ABI/cross-compile
risk in the plugin. Kept in reserve: if we ever must build on Linux, **clang-cl + xwin targeting
`i686-pc-windows-msvc`** is the ABI-safe path, with MinGW cross as the quick-but-riskier fallback.

## Consequences

- The M0 scaffold (VS2022 + Winsock + native `.asi`) is correct as-is; no rework.
- Toolchain to install on the Windows boot: **GTA SA v1.0 US** (downgrade Steam copy), **Ultimate
  ASI Loader (x86)**, **Visual Studio 2022** (Desktop C++), **DK22Pac Plugin SDK**, plus Python +
  PyTorch/CUDA + Stable-Baselines3 for the ML side.
- Get **Claude Code onto the Windows boot** so build/launch/test happens directly.

## Sources

Linux-toolchain verification (reference only): DK22Pac/plugin-sdk wiki + `premake5.lua` + issue #104
(MinGW path); `shared/PluginBase.h`/`Patch.h` (thiscall call templates, injector RegPack hooks);
Jake-Shadle/xwin and ProGTX/clang-msvc-sdk (clang-cl MSVC-ABI cross-compile); GCC MinGW thiscall
ABI notes; GTAmodding/re3 (RenderWare builds with GCC/ClViewer as native binary, not an ASI).
