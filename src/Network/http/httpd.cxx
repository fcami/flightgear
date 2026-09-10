// httpd.cxx -- FGHttpd base class definitions and the createInstance() factory
//
// Originally written by Torsten Dreyer, started April 2014
//
// SPDX-FileCopyrightText: 2014 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "HttplibHttpd.hxx"
#include "httpd.hxx"

using std::string;

namespace flightgear {
namespace http {

const char * PROPERTY_ROOT = "/sim/http";

FGHttpd * FGHttpd::createInstance(SGPropertyNode_ptr configNode)
{
    // only create a server if a port has been configured
    if (!configNode.valid()) return NULL;
    string port = configNode->getStringValue("options/listening-port", "");
    if (port.empty()) return NULL;

    return new flightgear::HttplibHttpd(configNode);
}

} // namespace http
} // namespace flightgear
