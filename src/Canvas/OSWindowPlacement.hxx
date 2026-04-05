// SPDX-FileCopyrightText: 2026 FlightGear Contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <simgear/canvas/CanvasPlacement.hxx>
#include <simgear/canvas/canvas_fwd.hxx>
#include <osg/Camera>
#include <osg/Image>
#include <osg/Texture2D>
#include <osg/ref_ptr>
namespace osgViewer { class View; }
namespace canvas {
class OSWindowPlacement : public simgear::canvas::Placement {
public:
    OSWindowPlacement(SGPropertyNode* node, simgear::canvas::CanvasPtr canvas);
    ~OSWindowPlacement() override;
    bool isValid() const { return _valid; }
    bool childChanged(SGPropertyNode* child) override;
private:
    simgear::canvas::CanvasWeakPtr _canvas;
    osg::ref_ptr<osgViewer::View> _view;
    osg::ref_ptr<osg::Image> _image;
    osg::ref_ptr<osg::Texture2D> _localTex;
    osg::ref_ptr<osg::Camera::DrawCallback> _readbackCb;
    bool _valid{false};
};
simgear::canvas::Placements
addOSWindowPlacement(SGPropertyNode*, simgear::canvas::CanvasPtr);
}
