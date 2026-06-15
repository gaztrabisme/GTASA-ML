# CLAUDE.md

Guidance for Claude Code (and humans) working in this repo.

## What this project is

An RL/DL "gym" to train **GTA: San Andreas soldier NPCs to fight better**. We wrap the running
game as a `gymnasium.Env` via a game↔Python bridge, read combat state, inject actions through the
ped **task system**, shape a reward, and train with PPO (Stable-Baselines3), including
soldier-vs-soldier self-play. End goal: research first, then ship as a playable mod.

> Research project against a personal, locally-modded copy of GTA SA. See the legal note in
> `docs/00-context-and-assumptions.md` before considering public distribution (a GTA-derived ML
> project was once taken down over Take-Two licensing).

## Read these first (project memory)

The design rests on a verification pass, not assumptions. Before proposing architecture changes,
read:

- `docs/00-context-and-assumptions.md` — verified engine/toolchain facts, corrections, prior art.
- `docs/01-environment-decision.md` — why we build on **native Windows**.
- `docs/02-windows-setup.md` — top-to-bottom Windows setup + the M0 acceptance test.
- `protocol/messages.md` — the bridge wire format (single source of truth).

## Hard constraints (verified — do not re-derive from memory)

- **Target game = GTA SA v1.0 US (HOODLUM).** All SDK/addresses assume it. 32-bit process.
- **NPCs are task-driven, NOT gamepad-driven.** `CPad` controls only the player ped. NPC control
  goes through the task system (`CTaskComplexKillPedOnFoot`, `CTaskSimpleUseGun`, go-to-coord).
  "Low-level" for NPCs = task primitives, not stick input.
- **~50-unit camera-centric world.** Peds are culled beyond ~54 units of the camera. No
  map-tiled parallel arenas — parallelism comes from many soldiers per fight + multiple game
  *processes* (SB3 vectorized envs).
- **Time-scale acceleration caps ~3–4×** (per-frame timestep clamped ≤3.0). Not the main throughput lever.
- **Ped pool cap = 140.**
- **Deployment inference:** no x86 prebuilt ONNX and no in-process SA prior art → external-process
  inference is the baseline for M6, not in-process ONNX.

## Environment & toolchain

- **Native Windows.** Visual Studio 2022 (Desktop C++), DK22Pac Plugin SDK, Ultimate ASI Loader
  (x86). Plugin builds **Win32/x86** (SA is 32-bit) — never x64.
- Build the plugin with the SDK's VS project template (recommended) or `plugin/CMakeLists.txt`.
- Python 3.11+ in `.venv`. M0 client is pure stdlib; ML deps (torch/sb3/gymnasium) only from M4.
- **None of the C++/game side can be built or run in a Linux/CI environment** — only on the
  Windows boot with the game installed.

## Repo layout

```
plugin/        C++ ASI bridge plugin (Plugin SDK). src/ + build files + README.
gym/           Python gymnasium.Env (later).
train/         SB3 PPO configs / self-play / checkpoints (later).
deploy/        ONNX export + in-game inference notes (later).
protocol/      Wire format: messages.md (keep both sides in sync with it).
scripts/       Tooling, e.g. m0_heartbeat_client.py.
docs/          Numbered design + decision docs.
```

## Roadmap (current: M0)

M0 hello-bridge → M1 read world state → M2 act + deterministic reset → M3 gym wrapper →
M4 train (high-level tactics, self-play) → M5 finer control (task primitives) → M6 ship.

- **M0 is scaffolded; not yet built/run.** Next concrete action is the Windows build + the M0
  acceptance test in `docs/02-windows-setup.md`. Record the heartbeat `msg/s` — it bounds the RL
  step rate and shapes M1+.
- Don't build later milestones on the bridge until M0 is proven on the real game.

## Conventions & workflow

- **Bridge:** plugin = TCP server (`127.0.0.1:7777`), Python = client. Length-prefixed JSON
  (`[uint32 LE length][JSON]`). Request–reply with backpressure for the eventual lockstep `step()`
  (prior art shows fire-and-forget overflows the queue).
- **Plugin frame hook:** `Events::gameProcessEvent` (per-frame). Keep the M0 path non-blocking;
  the intentional blocking lockstep arrives at M3.
- **ABI caution:** we're on MSVC precisely to match the game's ABI. Be wary of by-value
  struct/class returns from SDK methods if a non-MSVC toolchain is ever introduced.
- **Git:** develop on `claude/gta-sa-npc-combat-ml-8deecr`. Commit with clear messages; push with
  `git push -u origin <branch>`. Do not open PRs unless asked.
- **Style:** match surrounding code; keep comments at the density of the file you're editing.
- When changing the protocol, update `protocol/messages.md` first, then both sides.
