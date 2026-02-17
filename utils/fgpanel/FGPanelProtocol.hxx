// SPDX-FileCopyrightText: 2011 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <simgear/structure/subsystem_mgr.hxx>
#include <simgear/props/props.hxx>
#include <simgear/io/iochannel.hxx>

class PropertySetter;

typedef std::vector<PropertySetter*> PropertySetterVector;

class FGPanelProtocol : public SGSubsystem {
public:
    FGPanelProtocol (SGPropertyNode_ptr a_Root);
    virtual ~FGPanelProtocol ();

    // Subsystem API.
    void init() override;
    void reinit() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "panel-protocol"; }

private:
    SGPropertyNode_ptr root;
    SGIOChannel *io;
    PropertySetterVector propertySetterVector;
};
