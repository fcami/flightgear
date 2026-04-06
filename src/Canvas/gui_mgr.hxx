// Canvas gui/dialog manager
//
// SPDX-FileCopyrightText: 2012 Thomas Geymayer <tomgey@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <simgear/canvas/canvas_fwd.hxx>
#include <simgear/canvas/elements/CanvasGroup.hxx>
#include <simgear/props/PropertyBasedMgr.hxx>
#include <simgear/props/propertyObject.hxx>

#include <osg/ref_ptr>
#include <osg/Geode>
#include <osg/MatrixTransform>

namespace osgViewer { class View; }
namespace osg { class Camera; }

namespace osgGA
{
  class GUIEventAdapter;
}

class GUIEventHandler;
class GUIPickCallback;
class GUIMgr : public SGSubsystem
{
public:
    GUIMgr();

    // Subsystem API.
    void init() override;
    void shutdown() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "CanvasGUI"; }

    simgear::canvas::WindowPtr createWindow(const std::string& name = "");

    /**
     * Get simgear::canvas::Group containing all windows
     */
    simgear::canvas::GroupPtr getDesktop();

    /**
     * Set the input (keyboard) focus to the given window.
     */
    void setInputFocus(const simgear::canvas::WindowPtr& window);

    /**
     * Grabs the pointer so that all events are passed to this @a window until
     * the pointer is ungrabbed with ungrabPointer().
     */
    bool grabPointer(const simgear::canvas::WindowPtr& window);

    /**
     * Releases the grab acquired for this @a window with grabPointer().
     */
    void ungrabPointer(const simgear::canvas::WindowPtr& window);

    /**
     * specify the osgViewer::View and Camera
     */
    void setGUIViewAndCamera(osgViewer::View* view, osg::Camera* cam);
protected:
    simgear::canvas::GroupPtr           _desktop;
    osg::ref_ptr<GUIEventHandler>       _event_handler;
    SGSharedPtr<GUIPickCallback> _pickCallback;
    osg::ref_ptr<osgViewer::View>       _viewerView;
    osg::ref_ptr<osg::Camera>           _camera;

    simgear::canvas::Placements
    addWindowPlacement( SGPropertyNode* placement,
                        simgear::canvas::CanvasPtr canvas );
};
