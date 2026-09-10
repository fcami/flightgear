// httpd.hxx -- FGHttpd, the base class for FlightGear's http daemon subsystem
//
// Written by Torsten Dreyer, started April 2014.
//
// SPDX-FileCopyrightText: 2014 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later
//
#pragma once

#include <simgear/structure/subsystem_mgr.hxx>
#include <simgear/props/props.hxx>


namespace flightgear {

namespace http {

extern const char * PROPERTY_ROOT;

class FGHttpd : public SGSubsystem
{
public:
    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "httpd"; }

    static FGHttpd * createInstance( SGPropertyNode_ptr configNode );
};

} // namespace http

} // namespace flightgear
