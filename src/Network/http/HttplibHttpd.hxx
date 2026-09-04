// HttplibHttpd.hxx -- a http daemon subsystem based on cpp-httplib
//
// Part of the Mongoose -> cpp-httplib migration; see
// MONGOOSE_TO_HTTPLIB_PLAN.md at the repository root for the phased plan
// this implementation follows.
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
 * See MONGOOSE_TO_HTTPLIB_PLAN.md, Phase 2, for the design this follows:
 * URI handlers are called directly from httplib's worker threads for
 * ordinary property-tree access (relying on SGPropertyNode's own internal
 * locking), with an explicit main-thread hop only for the one operation
 * that isn't safe off the main thread (installing the screenshot handler's
 * osg::Camera::DrawCallback, and executing RunUriHandler's commands via
 * SGCommandMgr::execute()).
 *
 * WebSocket support (/PropertyListener, /PropertyTreeMirror/...) is
 * implemented too -- see HttplibHttpd.cxx and Phase 3 of
 * MONGOOSE_TO_HTTPLIB_PLAN.md for that design.
 *
 * The cpp-httplib types are kept out of this header (pImpl) so that
 * <httplib.h> -- a large single-header library -- isn't pulled into every
 * translation unit that just needs to know about FGHttpd.
 */
class HttplibHttpd : public http::FGHttpd
{
public:
    explicit HttplibHttpd(SGPropertyNode_ptr configNode);
    ~HttplibHttpd() override;

    // Subsystem API.
    //
    // SGSubsystem::bind() runs before init() (see subsystem_mgr.hxx), so
    // -- unlike MongooseHttpd, which puts everything in init() since its
    // own bind() is a noop -- all setup and the actual listen() happen in
    // bind() here; init() is not overridden.
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
