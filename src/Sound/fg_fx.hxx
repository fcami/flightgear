// fg_fx.hxx -- Sound effect management class
//
// Started by David Megginson, October 2001
// (Reuses some code from main.cxx, probably by Curtis Olson)
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2001 David Megginson

#pragma once

#include <simgear/compiler.h>

#include <vector>

#include <simgear/structure/subsystem_mgr.hxx>
#include <simgear/props/props.hxx>
#include <simgear/sound/sample_group.hxx>

class SGXmlSound;
using SGXmlSoundRef = SGSharedPtr<SGXmlSound>;

/**
 * Generator for FlightGear model sound effects.
 *
 * This module uses a FGSampleGroup class to generate sound effects based
 * on current flight conditions. The sound manager must be initialized
 * before this object is.
 *
 *    This module will load and play a set of sound effects defined in an
 *    xml file and tie them to various property states.
 */
class FGFX : public SGSampleGroup
{

public:

    FGFX ( const std::string &refname, SGPropertyNode *props = 0 );
    virtual ~FGFX ();

    void init();
    void update (double dt) override;
    void shutdown();

private:

    bool _active;
    bool _is_aimodel;
    SGSharedPtr<SGSampleGroup> _avionics;
    SGSharedPtr<SGSampleGroup> _atc;

    std::vector<SGXmlSoundRef> _xmlSounds;

    SGPropertyNode_ptr _props;
    SGPropertyNode_ptr _enabled;
    SGPropertyNode_ptr _volume;
    SGPropertyNode_ptr _avionics_enabled;
    SGPropertyNode_ptr _avionics_volume;
    SGPropertyNode_ptr _avionics_ext;
    SGPropertyNode_ptr _internal;
    SGPropertyNode_ptr _atc_enabled;
    SGPropertyNode_ptr _atc_volume;
    SGPropertyNode_ptr _atc_ext;
    SGPropertyNode_ptr _machwave_active;
    SGPropertyNode_ptr _machwave_volume;
};
