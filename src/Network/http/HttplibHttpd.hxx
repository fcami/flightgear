// HttplibHttpd.hxx -- a http daemon subsystem based on cpp-httplib
//
// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "httpd.hxx"

#include <memory>

namespace flightgear {

/**
 * A FGHttpd implementation based on cpp-httplib.
 *
 */
class HttplibHttpd : public http::FGHttpd
{
public:
    explicit HttplibHttpd(SGPropertyNode_ptr configNode);
    ~HttplibHttpd() override;

    // Subsystem API.
    void bind() override;
    void unbind() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "httplib-httpd"; }

private:
    class HttplibHttpdPrivate;
    std::unique_ptr<HttplibHttpdPrivate> d;
};

} // namespace flightgear
