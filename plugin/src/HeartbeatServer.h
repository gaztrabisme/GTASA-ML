// HeartbeatServer.h
//
// Minimal, header-only, NON-BLOCKING TCP server for the M0 hello-bridge.
//
// Design (see protocol/messages.md):
//   - Plugin is the server; one client (the Python gym side) connects.
//   - Everything is non-blocking and polled from inside the per-frame game hook,
//     so it never stalls the game thread. No background threads.
//   - Messages are length-prefixed JSON: [uint32 LE length][JSON bytes].
//
// This deliberately handles exactly ONE client at a time, which is all M0 needs.
// When the client disconnects we go back to accepting a new one.

#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

class HeartbeatServer {
public:
    HeartbeatServer() = default;
    ~HeartbeatServer() { Shutdown(); }

    // Create the listening socket. Returns false on failure (call GetLastErrorCode()).
    bool Start(unsigned short port = 7777) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            m_lastError = WSAGetLastError();
            return false;
        }
        m_wsaStarted = true;

        m_listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listenSock == INVALID_SOCKET) {
            m_lastError = WSAGetLastError();
            return false;
        }

        // Allow quick rebind after a restart.
        BOOL reuse = TRUE;
        setsockopt(m_listenSock, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (bind(m_listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            m_lastError = WSAGetLastError();
            return false;
        }
        if (listen(m_listenSock, 1) == SOCKET_ERROR) {
            m_lastError = WSAGetLastError();
            return false;
        }

        SetNonBlocking(m_listenSock);
        m_started = true;
        return true;
    }

    // Call once per game frame. Accepts a pending client if none is connected.
    // Returns true if a client is currently connected.
    bool Poll() {
        if (!m_started) return false;

        if (m_clientSock == INVALID_SOCKET) {
            SOCKET s = accept(m_listenSock, nullptr, nullptr);
            if (s != INVALID_SOCKET) {
                SetNonBlocking(s);
                m_clientSock = s;
            }
        }
        return m_clientSock != INVALID_SOCKET;
    }

    // Send one length-prefixed JSON message to the connected client (best-effort).
    // Drops the client on a hard error so Poll() can re-accept. Returns true if sent.
    bool SendJson(const std::string& json) {
        if (m_clientSock == INVALID_SOCKET) return false;

        const uint32_t len = static_cast<uint32_t>(json.size());
        char header[4];
        header[0] = static_cast<char>(len & 0xFF);
        header[1] = static_cast<char>((len >> 8) & 0xFF);
        header[2] = static_cast<char>((len >> 16) & 0xFF);
        header[3] = static_cast<char>((len >> 24) & 0xFF);

        if (!SendAll(header, 4)) return false;
        if (!SendAll(json.data(), static_cast<int>(json.size()))) return false;
        return true;
    }

    bool HasClient() const { return m_clientSock != INVALID_SOCKET; }
    int  GetLastErrorCode() const { return m_lastError; }

    void Shutdown() {
        DropClient();
        if (m_listenSock != INVALID_SOCKET) { closesocket(m_listenSock); m_listenSock = INVALID_SOCKET; }
        if (m_wsaStarted) { WSACleanup(); m_wsaStarted = false; }
        m_started = false;
    }

private:
    static void SetNonBlocking(SOCKET s) {
        u_long mode = 1;
        ioctlsocket(s, FIONBIO, &mode);
    }

    void DropClient() {
        if (m_clientSock != INVALID_SOCKET) { closesocket(m_clientSock); m_clientSock = INVALID_SOCKET; }
    }

    // Best-effort full send for a non-blocking socket. On a real error, drop the client.
    bool SendAll(const char* data, int total) {
        int sent = 0;
        while (sent < total) {
            int n = send(m_clientSock, data + sent, total - sent, 0);
            if (n == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK) {
                    // Send buffer full this frame. For M0 heartbeats we just give up on
                    // this message rather than spin/block the game thread.
                    return false;
                }
                m_lastError = err;
                DropClient();
                return false;
            }
            sent += n;
        }
        return true;
    }

    bool   m_wsaStarted = false;
    bool   m_started    = false;
    SOCKET m_listenSock = INVALID_SOCKET;
    SOCKET m_clientSock = INVALID_SOCKET;
    int    m_lastError  = 0;
};
