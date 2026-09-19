// httpd.cxx -- FGHttpd base class definitions and the createInstance() factory
//
// Originally written by Torsten Dreyer, started April 2014
//
// SPDX-FileCopyrightText: 2014 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "HttplibHttpd.hxx"
#include "httpd.hxx"

namespace flightgear {
namespace http {

const char * PROPERTY_ROOT = "/sim/http";

FGHttpd * FGHttpd::createInstance(SGPropertyNode_ptr configNode)
{
    // enabled by default: it listens on loopback-only unless overridden,
    // so it's safe to always start.
    // --disable-httpd (or setting options/enabled=false) turns it off.
    if (!configNode.valid()) {
        return nullptr;
    }

    if (!configNode->getBoolValue("options/enabled", true)) {
        return nullptr;
    }

    return new flightgear::HttplibHttpd(configNode);
}

} // namespace http
} // namespace flightgear
