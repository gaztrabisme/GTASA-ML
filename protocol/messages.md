# Bridge protocol

The game (C++ ASI plugin) and the Python side talk over a TCP socket.
This file is the **single source of truth** for the wire format. Keep both sides in sync with it.

## Framing

Every message is **length-prefixed JSON**:

```
[4 bytes: uint32 little-endian payload length N][N bytes: UTF-8 JSON]
```

Rationale: trivially debuggable, no schema compiler, and the length prefix avoids the
"where does one message end" problem on a stream socket. We can swap the JSON body for a
binary/struct layout later without changing the framing.

## Roles

- **Plugin = TCP server**, listens on `127.0.0.1:7777` (configurable).
- **Python = TCP client**, connects to it.

This matches the DeepGTAV pattern and sets up the eventual "Python drives the loop" design:
later, Python sends an `action`/`step` request and the plugin replies with an `observation`.

---

## M0 — heartbeat (current milestone)

Goal: prove the plugin loads, the per-frame hook fires, and bytes reach Python. **No ML, no peds.**

Direction: **plugin → Python**, one message per game frame (best-effort, non-blocking).

```jsonc
{
  "type": "heartbeat",
  "frame": 12345,        // monotonic frame counter since plugin load
  "wall_ms": 1718459000  // plugin wall-clock ms (std::chrono), for measuring step rate
}
```

Python just connects and prints these. Success = a steady stream of heartbeats with an
increasing `frame` while the game runs.

---

## Reserved for later milestones (not implemented yet)

Documented here so the schema grows coherently; do **not** rely on these until their milestone.

- **M1 `observation`** (plugin → Python): per-soldier state — health, armour, position, heading,
  weapon, ammo, nearest-enemy info, line-of-sight flags.
- **M2 `action`** (Python → plugin): tactical command or task primitive per soldier
  (e.g. `engage`, `take_cover`, or `{aim:[x,y,z], fire:true}`).
- **M2 `reset`** (Python → plugin): spawn/teleport/set-health to re-init a deterministic episode.
- **M3 `step`** (Python → plugin) / **`obs`** (plugin → Python): the synchronous lockstep round-trip
  that turns the real-time game into a turn-based env.
