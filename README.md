# GTASA-ML

Training **GTA: San Andreas soldier NPCs to fight better** with reinforcement / deep learning —
by building an RL "gym" around the running game.

> ⚠️ Research project. Targets a personal, locally-modded copy of GTA: San Andreas (v1.0 US).
> See the legal note in the context doc before considering any public distribution.

## Status

Pre-scaffold. We are in the **context-gathering / assumption-verification** phase — confirming
the engine and toolchain facts the architecture depends on *before* writing code.

- 📄 [`docs/00-context-and-assumptions.md`](docs/00-context-and-assumptions.md) — verified engine
  facts, corrections, prior art, and how they reshape the plan. **Start here.**

## Approach (high level)

1. Wrap the running game as a `gymnasium.Env` via a game↔Python bridge.
2. Read NPC/enemy state, inject actions through the ped **task system**, derive a combat reward.
3. Train with PPO (Stable-Baselines3); soldier-vs-soldier self-play.
4. Roadmap: M0 hello-bridge → M1 read world → M2 act/reset → M3 gym → M4 train → M5 finer control → M6 ship.

See the context doc for the verified constraints that shape each stage (notably: ~50-unit
camera-centric world, 140-ped cap, ~3–4× time-scale ceiling, and NPCs being task-driven — not
gamepad-driven).
