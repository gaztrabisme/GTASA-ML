# 00 — Context & Verified Assumptions

> **Status:** Living document. Captures the verification pass done *before* writing any
> code, so the architecture rests on confirmed engine facts rather than memory.
> Each claim is labelled **CONFIRMED**, **CORRECTED**, or **UNVERIFIED**, with sources.
> Date of verification pass: 2026-06-15.

## Project goal (recap)

Build a "gym" to train **GTA: San Andreas soldier NPCs to fight better** with RL/DL.

Decisions already made with the user:

| Decision | Choice |
| --- | --- |
| Control granularity | **Hybrid** — high-level tactics first, finer control later |
| End goal | **Research first, then ship as a playable mod** |
| User's modding background | New to GTA modding (strong on ML) |
| Hardware | Single PC + NVIDIA GPU |

## How this was verified

Four parallel research agents checked four clusters of assumptions against authoritative
sources: the GTA modding wiki (gtamods.com), the reverse-engineered headers in
**DK22Pac/plugin-sdk**, the **gta-reversed** decompilation project, **ThirteenAG/Ultimate-ASI-Loader**,
**cleolibrary/CLEO5**, GTAForums, the Sanny Builder opcode database, and GitHub/arXiv for prior art.

---

## 1. Toolchain & loading

| # | Claim | Verdict |
| --- | --- | --- |
| 1 | Ecosystem targets **v1.0 US ("HOODLUM")**; Steam/RGL versions are not mod-compatible and should be downgraded | **CONFIRMED** |
| 2 | **Ultimate ASI Loader** (proxy DLL) is the standard `.asi` loader; scans game root + `scripts`/`plugins`/`update` | **CONFIRMED** |
| 3 | **Plugin SDK** (DK22Pac) supports III/VC/SA, needs **Visual Studio 2022 + C++**, exposes `CPed`/`CWorld`/`CTask`/`CTimer` etc. | **CONFIRMED** |
| 4 | **CLEO** is the opcode/script host (current **CLEO 5.4.0**, runs on top of an ASI loader); CLEO5 supports custom `.cleo` plugins/opcodes | **CONFIRMED** |
| 5 | `gta_sa.exe` is **32-bit/x86** → all injected plugins must be x86 | **CONFIRMED** |
| 6 | ONNX Runtime can run *inside* a 32-bit SA plugin | **CORRECTED** |

**On (6) — deployment inference path.** Microsoft ships **no official prebuilt x86 ONNX
Runtime**; a 32-bit build must be cross-compiled from source, and there is **no prior art**
running ORT in-process in any GTA SA plugin. All existing GTA ML projects run the model in an
**external process**. → For M6 (ship), prefer **external-process inference** or a tiny
hand-rolled/embedded net; treat in-process ORT as a stretch, not a baseline.

**Practical setup:** GTA SA v1.0 US + Ultimate ASI Loader (x86) + Plugin SDK on VS2022.
Avoid spaces in SDK paths; configure the SDK env vars per its wiki.

---

## 2. Engine internals

| # | Claim | Verdict |
| --- | --- | --- |
| 1 | Streaming/population is **camera-centric**; far entities are culled, not simulated | **CONFIRMED** |
| 2 | **Ped pool hard cap = 140** (`CPools::Initialise`) | **CONFIRMED** |
| 3 | Global **time-scale** `CTimer::ms_fTimeScale` (0xB7CB64, default 1.0); >1 speeds sim; opcode `015D` | **CONFIRMED** |
| 4 | Per-frame hook via `Events::gameProcessEvent` / engine `CGame::Process` | **CONFIRMED** |
| 5 | **Interior/area IDs** isolate visibility + collision + LoS at shared coords | **CONFIRMED** |
| 5b | Off-area entities are *fully un-simulated* | **PARTIALLY UNVERIFIED** |
| 6 | Render can be skipped while still stepping the world (headless-ish) | **CONFIRMED (bounded)** |

**Design-shaping facts:**

- **~50-unit world.** Peds are created in a ~10–50 unit ring around the camera and **removed
  beyond ~54.5 units** (or ~25 units if off-screen, after a few-second timeout). → The arena
  and *every* combatant must live within ~50 units of the camera. **"Spread fights across the
  6×6 km map" is impossible** — only the area under the camera is alive. A few small sub-arenas
  *clustered* near the camera do fit.
- **Time-scale acceleration is capped ~3–4×.** Per-frame timestep is clamped to **≤ 3.0**, and
  `time_scale` > 4.0 yields no further speed-up; some events override it. → Acceleration is a
  modest knob, **not** the main throughput lever.
- **Interiors as arena dividers.** Different area codes prevent peds seeing/colliding/shooting
  across them — ideal for non-interfering concurrent arenas. But whether off-area entities are
  truly frozen is unconfirmed (rendering is culled; pool `ProcessControl()` may still run). →
  **M1 experiment** before relying on it.
- **Render-skip** is feasible (sim and present are separable) but bounded by the same timestep
  clamp, and skips render-list bookkeeping (alpha-fade, RW-object GC). → Defer to M4+ as a speed
  knob, not a requirement.

---

## 3. Ped AI: reading & control

| # | Claim | Verdict |
| --- | --- | --- |
| 1 | Task system (`CPedIntelligence`→`CTaskManager`); combat task **`CTaskComplexKillPedOnFoot`**, gun task **`CTaskSimpleUseGun`** | **CONFIRMED / CORRECTED** |
| 2 | Read health `m_fHealth`(0x540), armour(0x548), weapon/ammo (`m_aWeapons[13]`), heading, position | **CONFIRMED** |
| 3 | `CWorld::ProcessLineOfSight` / `GetIsLineOfSightClear` / `FindObjectsInRange` | **CONFIRMED** |
| 4 | Disable native AI by clearing tasks each frame (`FlushImmediately`) + passive decision maker (`060B`) | **CONFIRMED (no single toggle)** |
| 5 | Gamepad (`CPad`) emulation can drive arbitrary peds | **CORRECTED — player ped ONLY** |
| 6 | Spawn / teleport / set-health / give-weapon for deterministic reset | **CONFIRMED** |

**The pivotal correction (5) — this reshapes the hybrid plan:**

> **`CPad` input drives only the player ped.** Generic NPC peds are controlled **exclusively
> through the task system**. There is no stick/button input for NPCs.

Consequences for "hybrid" control:

- **NPC soldiers (the squad / self-play case):** the finest control available is **task
  primitives** — `CTaskSimpleUseGun(target, aimPoint, fireCommand, burstLen)` for aim+fire,
  go-to-coord / seek tasks for movement, plus higher-level `CTaskComplexKillPedOnFoot`. This is
  *granular* (you choose aim point and when to fire) but it is **task-level, not stick-level**.
- **True raw motor control** (move-vector + analog aim + trigger) is only possible on the
  **single player ped** via `CPad`. That means a low-level-motor agent can only train **one
  agent as the player** — it cannot do NPC-vs-NPC self-play at the stick level.

→ **Revised meaning of "hybrid":**
1. **Stage A (NPC, high-level):** policy picks tactics → executed via tasks. Self-play of many
   soldiers works.
2. **Stage B (NPC, fine-grained):** policy emits **task primitives** (where to move, what to aim
   at, when to fire). Still self-play-capable. *This replaces the original "stick-level for NPCs"
   idea, which the engine does not allow.*
3. **Stage C (optional, player ped):** if we want genuine raw-motor control, train a single agent
   as the player via `CPad`. Single-agent only.

**Disabling native AI:** no master switch. Each control step, `FlushImmediately()` the ped's
tasks (or overwrite the primary task) and/or assign a passive decision maker (`060B`), else the
event scanners re-populate default behaviour. `04D7` freezes a ped in place (useful for reset).

---

## 4. Prior art

| Topic | Finding |
| --- | --- |
| GTA combat RL (any title) | **None found** — novel territory |
| Synchronous pause/step bridge (any GTA) | **None found** — every project runs real-time/async; our lockstep `step()` would be new |
| GTA V data/RL bridges | **DeepGTAV** (TCP:8000 → ZeroMQ forks), **GTAV-RewardHook** (REST + reward-collapse), DeepDrive/Universe |
| GTA SA ML | Screen-capture + keypress self-driving only (saksham36, saurabh241930) |
| GTA SA state extraction | Memory-read pattern proven: `go-gta-sa-memory-reader`, SA trainers, GTAMods `CPed` layout |
| Time-scale for faster-than-real-time training | Exists (`SET_TIME_SCALE`); **no ML project actually exploits it** — I/O is the usual bottleneck |

**Reusable patterns to adopt:**
- ScriptHook-style **socket server on the game side**; use **request–reply with backpressure**
  (DeepGTAV's ZeroMQ fork reported queue overflow / out-of-order msgs with fire-and-forget).
- **Reward-accumulate-then-collapse** to a scalar per step (GTAV-RewardHook).
- **External-process state read** (`pymem`/`ReadProcessMemory` against the `CPed` layout) is a
  viable **lower-barrier alternative to a C++ ASI plugin** for someone new to modding.
- Decouple heavy processing from the hot loop; expect **I/O, not physics, to bottleneck**.

**⚠️ Legal flag (matters only for the "ship/publish" goal):** the OpenAI-Universe GTA V project
(DeepDrive) was **taken down over Take-Two/Rockstar licensing**. A personal research mod is the
norm in this community, but public distribution of a GTA-derived ML product carries real risk.

---

## 5. What changed in the plan because of this

1. **Two viable bridge architectures** — decide at M0:
   - **(a) C++ ASI plugin** opening a socket (clean, full task access, the long-term path), or
   - **(b) External Python process** doing `ReadProcessMemory` + writing tasks/inputs (far lower
     barrier for an ML-first person; proven by existing SA tools). Could prototype the loop in
     (b) and migrate hot paths to (a).
2. **"Low-level for NPCs" = task primitives, not stick input.** Raw motor control is player-ped,
   single-agent only. Hybrid re-scoped into Stages A/B/C above.
3. **Throughput plan:** many soldiers per fight (free) + **multiple game processes** vectorised
   in SB3; time-scale only a ~3–4× bonus. Not map-tiling.
4. **Arena:** custom/empty flat zone, everything within ~50 units of the camera; interiors as
   possible dividers (pending the M1 isolation test).
5. **Deployment (M6):** external-process inference is the baseline; in-process ONNX is a stretch.

## 6. Open questions to resolve empirically (M1 experiments)

- [ ] Do entities in a **non-current interior still get `ProcessControl()`** (can interiors host
      concurrent live arenas)?
- [ ] Real wall-clock **step rate** of a freeze→apply→advance→read loop at time-scale 1× and ~3×.
- [ ] How many **concurrent game processes** the single PC sustains (CPU/RAM/GPU).
- [ ] Does `FlushImmediately()` each frame **reliably suppress** native combat AI for soldiers?
- [ ] Min viable **observation read latency** via socket vs `ReadProcessMemory`.

---

## Sources

**Toolchain:** DK22Pac/plugin-sdk (`GameVersion.h`, `plugin_sa/game_sa`, IDE wiki) ·
ThirteenAG/Ultimate-ASI-Loader · cleolibrary/CLEO5 + CLEO 5 SDK (cleo.li) ·
microsoft/onnxruntime (build-from-source; no x86 binaries) · HuuHuy227/Self-Driving-Gta-Sa.

**Engine:** gta-reversed (`Population.cpp`, `Pools.cpp`, `Timer.cpp`, `Renderer.cpp`, `Entity.h`) ·
gtamods.com (Resource Streaming, Interior, 015D, Memory Addresses (SA), Renderhook) ·
plugin-sdk `CTimer.cpp`/`CEntity.h`/GPS example · MTA wiki (Streaming, GC) · speedrun.com frame-limiter guide.

**Ped AI:** plugin-sdk `CPed.h`, `CPedIntelligence.h`, `CTaskManager`, `CTaskComplexKillPedOnFoot`,
`CTaskSimpleUseGun`, `CWorld`, `CWeapon.h`, `CPad.h`, `CPlayerPed` · SASCM opcodes (05E2, 060B, 04D7,
009A, 00A1, 0223, 01B2) · gtag.sannybuilder.com.

**Prior art:** aitorzip/DeepGTAV · David0tt/DeepGTAV · f1recracker/GTAV-RewardHook ·
OSSDC/deepdrive-universe + deepdrive.io · arXiv 1712.01397, 2502.12303 ·
saksham36/Reinforcement-Learning-with-GTA_SA · saurabh241930 (SA CNN bot) ·
nuriofernandez/go-gta-sa-memory-reader · gtamods.com Memory Addresses (SA) · gtamods.com SET_TIME_SCALE.
