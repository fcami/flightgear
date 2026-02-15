// FGMouseInput.hxx -- handle user input from mouse devices
//
// Written by Torsten Dreyer, started August 2009
// Based on work from David Megginson, started May 2001.
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2001 David Megginson <david@megginson.com>
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#pragma once

#include "FGCommonInput.hxx"

#include <memory>

#include <simgear/structure/subsystem_mgr.hxx>

// forward decls
namespace osgGA { class GUIEventAdapter; }

////////////////////////////////////////////////////////////////////////
// The Mouse Input Class
////////////////////////////////////////////////////////////////////////
class FGMouseInput : public SGSubsystem,
                     FGCommonInput
{
public:
    FGMouseInput();
    virtual ~FGMouseInput() = default;

    // Subsystem API.
    void init() override;
    void reinit() override;
    void shutdown() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "input-mouse"; }

    void doMouseClick (int b, int updown, int x, int y, bool mainWindow, const osgGA::GUIEventAdapter* ea);
    void doMouseMotion (int x, int y, const osgGA::GUIEventAdapter*);
    void doMouseLeave(const osgGA::GUIEventAdapter*);

    /**
     * @brief isRightDragToLookEnabled - test if we're in right-mouse-drag
     * to adjust the view direction/position mode.
     * @return
     */
    bool isRightDragToLookEnabled() const;

private:
    void processMotion(int x, int y, const osgGA::GUIEventAdapter* ea);

    bool isRightDragLookActive() const;

    class FGMouseInputPrivate;
    std::unique_ptr<FGMouseInputPrivate> d;
};
