# 02 — Windows Setup Checklist

Top-to-bottom setup for the Windows boot. Work through it in order; each section ends with a
**check** you should confirm before moving on. When all checks pass you can build & run M0.

> Environment is **native Windows** (see `docs/01-environment-decision.md`). Everything here is
> native — no Wine, no cross-compile.

---

## 0. Prerequisites at a glance

| Component | Why | Check |
| --- | --- | --- |
| GTA SA **v1.0 US (HOODLUM)** | Every SDK/address assumes it | `gta_sa.exe` ~14 MB; in-game version text shows 1.0 |
| Ultimate ASI Loader (x86) | Loads our `.asi` | A known test ASI loads |
| Visual Studio 2022 (Desktop C++) | Builds the plugin (MSVC ABI) | `cl.exe` available in x86 Native Tools prompt |
| DK22Pac Plugin SDK | Engine class wrappers | Sample plugin builds |
| Python 3.11+ | Bridge client now; ML later | `python --version` |
| Claude Code (Windows) | Direct build/test loop | `claude` runs in the repo |

---

## 1. Game: get a clean v1.0 US copy

The Steam/RGL copy is a newer, **non-moddable** version. You must downgrade to **v1.0 US**.

1. Install GTA SA from Steam.
2. **Copy the whole install folder elsewhere** first (downgrading + modding a Steam-managed
   folder invites Steam re-validating/overwriting files). Mod the *copy*.
3. Downgrade that copy to **v1.0 US (HOODLUM)**. Follow the **PCGamingWiki** "Grand Theft Auto:
   San Andreas → Essential improvements / Downgrade" instructions — it's the maintained,
   reputable reference for the current Steam→1.0 method and links the right files. Avoid random
   re-upload sites.
4. Recommended companion: install **SilentPatch** (community-standard stability/bugfix) after
   downgrading.

**Check:** launch the downgraded copy; the main-menu version string reads **1.0**. Note the full
path to this folder — call it `GAME_DIR` from here on.

> Why a separate copy: keeps Steam from clobbering modded files, and gives you a clean baseline
> to restore if a build wedges the game.

---

## 2. Ultimate ASI Loader

1. Download the **x86** build from the official repo releases:
   https://github.com/ThirteenAG/Ultimate-ASI-Loader (Releases → 32-bit).
2. Place the loader in `GAME_DIR`, renamed to a proxy DLL the game loads. For SA on **native
   Windows**, `dinput8.dll` works fine. (The `dinput8` problems documented online are
   **Wine-specific** and don't apply to us.)
   - Alternative proxy if you prefer the CLEO convention: `vorbisFile.dll`.
3. Make a `GAME_DIR\scripts\` folder (the loader scans `scripts/`, `plugins/`, and the root for
   `.asi` files).

**Check:** drop any known-good `.asi` (or just our M0 build later) into `GAME_DIR` or
`GAME_DIR\scripts\` and confirm it loads. If you install CLEO (also loads via the ASI loader)
and it works, the loader is good.

---

## 3. Visual Studio 2022

1. Install **Visual Studio 2022 Community**.
2. In the installer, select the **"Desktop development with C++"** workload (gives MSVC, the
   Windows SDK, and the build tools). The 32-bit toolset is included.

**Check:** open the **"x86 Native Tools Command Prompt for VS 2022"** and run `cl` — it should
print the compiler banner.

---

## 4. Plugin SDK (DK22Pac)

1. Clone it to a path **with no spaces** (e.g. `C:\dev\plugin-sdk`):
   ```bat
   git clone https://github.com/DK22Pac/plugin-sdk C:\dev\plugin-sdk
   ```
2. Follow the SDK wiki **"Installing development environment (IDE)"** to set up the environment
   variable it expects (so VS finds the SDK) and build the SDK libs:
   https://github.com/DK22Pac/plugin-sdk/wiki
3. Build the SDK's **SA** libraries (Release, Win32) once, so our plugin can link them.

**Check:** build one of the SDK's example SA plugins (e.g. the GPS example) → produces an `.asi`
without errors. *Get a sample building before touching our code* — it isolates SDK setup issues
from our code.

---

## 5. Build our M0 plugin

Use the SDK's **"Creating a new plugin"** VS template (lowest-risk path; see `plugin/README.md`):

1. Create a new **SA** plugin project from the SDK template.
2. Remove the template's stub `.cpp`; **add** our files:
   - `plugin/src/Main.cpp`
   - `plugin/src/HeartbeatServer.h`
3. Set configuration to **Release / Win32 (x86)**.
4. `Ws2_32.lib` is linked via a `#pragma comment(lib, ...)` in our header — no extra linker step.
5. Build → produces `GTASAML.asi` (or your chosen name).

(Optional CMake path documented in `plugin/CMakeLists.txt`; the VS template is recommended.)

**Check:** build succeeds and emits an `.asi`.

---

## 6. Python (bridge client now; ML later)

1. Install **Python 3.11+** (tick "Add to PATH").
2. Create a venv in the repo:
   ```bat
   python -m venv .venv
   .venv\Scripts\activate
   ```
3. **M0 needs nothing beyond the standard library** — `scripts/m0_heartbeat_client.py` is pure
   stdlib.
4. **Later (M4 training)** install the ML stack into the venv:
   ```bat
   pip install torch --index-url https://download.pytorch.org/whl/cu121   REM match your CUDA
   pip install stable-baselines3 gymnasium numpy
   ```
   (Defer this until we actually train; it's listed here so the checklist is complete.)

**Check:** `python --version` ≥ 3.11 and the venv activates.

---

## 7. Claude Code on Windows

Install Claude Code on the Windows boot and open it in the repo, so build/launch/test happens
directly instead of relaying errors. See https://code.claude.com/docs for the current installer.

**Check:** `claude` starts inside `GTASA-ML` and can run `git status`.

---

## 8. M0 acceptance test (the payoff)

1. Copy `GTASAML.asi` into `GAME_DIR` (or `GAME_DIR\scripts\`).
2. Start the client:
   ```bat
   .venv\Scripts\activate
   python scripts\m0_heartbeat_client.py
   ```
3. Launch the downgraded GTA SA and load into **actual gameplay** (not the menu).
4. **PASS:** the client prints a steady stream of heartbeats with an increasing `frame` and a
   `~N msg/s` rate.

**Record the `msg/s` number** — it's the first real measurement of bridge throughput and directly
bounds the RL step rate. Bring it back and we plan M1 around it.

---

## Troubleshooting quick map

| Symptom | Likely cause |
| --- | --- |
| Client never connects | Plugin didn't load (wrong game version / ASI loader inactive / built x64 not Win32) or port 7777 taken |
| Connects, no messages | Frame hook not firing — you're in a menu/loading screen, not gameplay |
| Game crashes on load | Game-version / SDK-address mismatch — re-verify v1.0 US |
| SDK won't build | Path has spaces, or env var / SDK libs not built (redo §4) |

## Reference

- Verified engine/toolchain facts: `docs/00-context-and-assumptions.md`
- Why Windows: `docs/01-environment-decision.md`
- Plugin build detail: `plugin/README.md`
- Wire format: `protocol/messages.md`
