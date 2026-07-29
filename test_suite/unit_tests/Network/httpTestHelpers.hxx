// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Network/http/HTTPRequest.hxx>
#include <Network/http/Websocket.hxx>
#include <Network/http/urihandler.hxx>

#include <string>
#include <vector>

namespace FGTestApi {
namespace http {

/**
 * Captures every frame written to a websocket during a test.
 * Only text (opcode 1) and binary (opcode 2) frames are stored;
 * control frames (ping/pong/close) are silently acknowledged.
 */
class MockWebsocketWriter : public flightgear::http::WebsocketWriter
{
public:
    int writeToWebsocket(int opcode, const char* data, size_t len) override
    {
        if (opcode == 1 || opcode == 2) {
            frames.emplace_back(data, len);
        }
        return static_cast<int>(len);
    }

    std::vector<std::string> frames;
};

/**
 * Minimal Connection stub for URI handler tests.
 * Stores key/value connection data (via the base class map) and
 * captures any raw bytes written via write().
 */
class MockConnection : public flightgear::http::Connection
{
public:
    void write(const char* data, size_t len) override
    {
        written.append(data, len);
    }

    std::string written;
};

/**
 * Convenience factory for building HTTPRequest objects in tests.
 */
inline flightgear::http::HTTPRequest buildRequest(
    const std::string& method,
    const std::string& uri,
    const std::string& content = {})
{
    flightgear::http::HTTPRequest req;
    req.Method = method;
    req.Uri = uri;
    req.Content = content;
    return req;
}

} // namespace http
} // namespace FGTestApi
