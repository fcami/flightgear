// fg_os_common.cxx -- common functions for fg_os interface
// implemented as an osgViewer
//
// SPDX-FileCopyrightText: 2007 Tim Moore <timoore@redhat.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config.h>

#include <osg/GraphicsContext>

#include <Viewer/renderer.hxx>
#include <Viewer/FGEventHandler.hxx>
#include "fg_os.hxx"
#include "globals.hxx"

// fg_os callback registration APIs
//

// Event handling and scene graph update is all handled by a
// manipulator. See FGEventHandler.cpp
void fgRegisterIdleHandler(fgIdleHandler func)
{
    globals->get_renderer()->getEventHandler()->setIdleHandler(func);
}

void fgRegisterKeyHandler(fgKeyHandler func)
{
    globals->get_renderer()->getEventHandler()->setKeyHandler(func);
}

void fgRegisterMouseClickHandler(fgMouseClickHandler func)
{
    globals->get_renderer()->getEventHandler()->setMouseClickHandler(func);
}

void fgRegisterMouseMotionHandler(fgMouseMotionHandler func)
{
    globals->get_renderer()->getEventHandler()->setMouseMotionHandler(func);
}

void fgRegisterMouseLeaveHandler(fgMouseLeaveHandler func)
{
    globals->get_renderer()->getEventHandler()->setMouseLeaveHandler(func);
}
