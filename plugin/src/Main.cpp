// Main.cpp — GTASA-ML bridge plugin, milestone M0 (hello-bridge).
//
// What this does:
//   - Registers with the GTA SA Plugin SDK frame events.
//   - Stands up a non-blocking TCP server (HeartbeatServer).
//   - Once per game frame, accepts a client if needed and sends a heartbeat JSON.
//
// What this intentionally does NOT do yet:
//   - Read any ped/world state (that's M1).
//   - Apply any actions or reset episodes (M2).
//   - Block the game for a synchronous step() (M3).
//
// Build/run instructions: see plugin/README.md.

#include "plugin.h"          // Plugin SDK (DK22Pac/plugin-sdk)
#include "HeartbeatServer.h"

#include <chrono>
#include <string>

using namespace plugin;

namespace {

constexpr unsigned short kPort = 7777;

long long WallClockMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

class GtaSaMlPlugin {
public:
    GtaSaMlPlugin() {
        // initGameEvent fires once the game is up and it's safe to touch game systems.
        Events::initGameEvent += []() {
            Instance().OnInit();
        };

        // gameProcessEvent fires every frame during game logic — our per-frame tick.
        Events::gameProcessEvent += []() {
            Instance().OnFrame();
        };
    }

    void OnInit() {
        m_serverUp = m_server.Start(kPort);
        // Note: no in-game logging set up yet. If m_serverUp is false, the most likely
        // cause is the port already being in use. We surface this via the heartbeat
        // simply never appearing; M1 will add proper on-screen/file logging.
    }

    void OnFrame() {
        if (!m_serverUp) return;

        ++m_frame;
        m_server.Poll();  // accept a client if one is waiting

        if (m_server.HasClient()) {
            // Hand-rolled JSON to avoid pulling a dependency into M0. Swap for a real
            // JSON lib once messages get richer (M1+).
            std::string msg =
                "{\"type\":\"heartbeat\",\"frame\":" + std::to_string(m_frame) +
                ",\"wall_ms\":" + std::to_string(WallClockMs()) + "}";
            m_server.SendJson(msg);
        }
    }

    static GtaSaMlPlugin& Instance() {
        static GtaSaMlPlugin inst;
        return inst;
    }

private:
    HeartbeatServer  m_server;
    bool             m_serverUp = false;
    unsigned long long m_frame  = 0;
};

// Force construction so the event handlers register at load time.
GtaSaMlPlugin& g_plugin = GtaSaMlPlugin::Instance();

}  // namespace
