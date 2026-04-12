// realwx_ctrl.cxx -- Process real weather data
//
// Written by David Megginson, started May 2002.
// Rewritten by Torsten Dreyer, August 2010
//
// SPDX-FileCopyrightText: 2002 David Megginson <david@megginson.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <simgear/structure/subsystem_mgr.hxx>
#include <simgear/props/props.hxx>

namespace Environment {

class RealWxController : public SGSubsystem
{
public:
    virtual ~RealWxController();

    static RealWxController * createInstance( SGPropertyNode_ptr rootNode );

    //SGSharedPtr<SGPropertyNode_ptr> getMetarProperties(const std::string& icao);
};

} // namespace Environment
