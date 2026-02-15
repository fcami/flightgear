// SPDX-FileCopyrightText: 2022 James Hogan <james@albanarts.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Implements 3D mouse cursor visible in the scene.
 */

#include "FGMouseCursor3D.hxx"

#include "FGDirectionCue3D.hxx"
#include "renderer.hxx"
#include "view.hxx"
#include "viewmgr.hxx"

#include <Main/fg_props.hxx>
#include <Model/acmodel.hxx>

#include <simgear/debug/ErrorReportingCallback.hxx>
#include <simgear/scene/model/placement.hxx>
#include <simgear/scene/util/OsgMath.hxx>
#include <simgear/scene/util/RenderConstants.hxx>
#include <simgear/scene/util/SGNodeMasks.hxx>
#include <simgear/scene/util/SGPickCallback.hxx>
#include <simgear/scene/util/SGReaderWriterOptions.hxx>

using namespace flightgear;

typedef FGRenderer::PickList PickList;

FGMouseCursor3D::FGMouseCursor3D()
{
    setName("3D cursor");

    osg::Switch* sw = new osg::Switch;
    addChild(sw);

    // Read cursor models from /sim/vr/config/cursors/cursor[]
    SGPropertyNode_ptr cursor3DNode = fgGetNode("/sim/vr/config/cursors", true);
    for (auto cursor : cursor3DNode->getChildren("cursor")) {
        std::string path = cursor->getStringValue("model/path", "");
        std::string cursorStr = cursor->getStringValue("cursor", "");
        unsigned int cursorId = (int)FGMouseCursor::cursorFromString(cursorStr.c_str());

        simgear::ErrorReportContext ec("cursor-model", path);

        SGPath resolvedPath = globals->resolve_aircraft_path(path);
        if (resolvedPath.isNull()) {
            simgear::reportFailure(simgear::LoadFailure::NotFound,
                                   simgear::ErrorCode::XMLModelLoad,
                                   "Failed to find 3D mouse cursor model",
                                   SGPath::fromUtf8(path));
            continue;
        }

        osg::Node* node = simgear::SGModelLib::loadModel(resolvedPath.utf8Str(),
                                                         globals->get_props());
        if (node) {
            // Add mapping from the cursor type to the model node
            if (cursorId >= _modelMapping.size())
                _modelMapping.resize(cursorId + 1, -1);
            _modelMapping[cursorId] = sw->getNumChildren();

            // And add the model node to the switch
            sw->addChild(node, false);
        }
    }

    // Don't allow the cursor to itself be picked, or it may try to pick itself!
    // Don't allow the aircraft to collide catastrophically with the cursor!
    setNodeMask(~SG_NODEMASK_PICK_BIT & ~SG_NODEMASK_TERRAIN_BIT);

    // Create direction cue object
    _cue = new FGDirectionCue3D(this);
}

void FGMouseCursor3D::setTargetGlobal(const SGVec3d& target, bool commit)
{
    // We need the current view
    auto* view_mgr = globals->get_subsystem<FGViewMgr>();
    if (!view_mgr)
        return;
    auto* view = view_mgr->get_current_view();
    if (!view)
        return;

    // Get aircraft pose
    auto* aircraft = globals->get_subsystem<FGAircraftModel>()->get3DModel();
    auto aircraftPosition = aircraft->getPosition();
    SGQuatd aircraftOrient = aircraft->getGlobalOrientation();

    // Update the cursor position
    osg::Matrix mat;
    SGVec3d targetAircraft = aircraftOrient.transform(target - SGVec3d::fromGeod(aircraftPosition));
    mat.setTrans(toOsg(targetAircraft));

    // Commit cursor position
    if (commit)
        _targetAircraft = targetAircraft;

    // Match the orientation to the view
    mat.setRotate(toOsg(inverse(aircraftOrient) * view->getViewOrientation()));

    // Scale to a fixed angular size
    double scale = length(target - view->getViewPosition());
    mat.preMultScale(osg::Vec3d(scale, scale, scale));
    setMatrix(mat);

    // Clear any pending recenter
    _recenterPending = false;

    // Add cursor to scene graph under aircraft
    if (!getNumParents())
        aircraft->add(this);
}

FGRenderer::PickList FGMouseCursor3D::update(bool forcePick)
{
    FGRenderer::PickList pickList;
    // We need the current view
    auto* view_mgr = globals->get_subsystem<FGViewMgr>();
    if (!view_mgr)
        return pickList;
    auto* view = view_mgr->get_current_view();
    if (!view)
        return pickList;

    bool visible = (_curModelId >= 0);
    bool motion = (_motion2d != SGVec2d(0.0, 0.0));
    // Should we pick to find the latest depth to the target?
    if (visible || motion || forcePick) {
        // Get aircraft pose
        auto* aircraft = globals->get_subsystem<FGAircraftModel>()->get3DModel();
        auto aircraftPosition = aircraft->getPosition();
        SGQuatd aircraftOrient = aircraft->getGlobalOrientation();

        // Transform target from aircraft space back into global space
        SGVec3d targetGlobal = SGVec3d::fromGeod(aircraftPosition) + aircraftOrient.backTransform(_targetAircraft);

        // Transform target into view space
        SGVec3d viewPosition = view->getViewPosition();
        SGVec3d vecGlobal = targetGlobal - viewPosition;
        if (motion || _recenterPending) {
            SGQuatd viewOrientation = view->getViewOrientation();
            SGVec3d targetView = viewOrientation.transform(vecGlobal);

            if (_recenterPending) {
                // Recenter the target in view space
                targetView = SGVec3d(0.0, 0.0, -1.0);
                _motion2d = SGVec2d(0.0, 0.0);
            } else if (motion) {
                // Rotate in view space based on 2D mouse motion
                // FIXME adaptive to DPI of screen?
                const double pixelAngle = SGMiscd::deg2rad(0.05);
                auto motionRotation = SGQuatd::fromEulerRad(0.0,
                                                            pixelAngle * _motion2d.x(),
                                                            -pixelAngle * _motion2d.y());
                targetView = motionRotation.transform(targetView);
                _motion2d = SGVec2d(0.0, 0.0);
            }

            // Transform altered target back from view space into global space
            vecGlobal = viewOrientation.backTransform(targetView);
        }

        // Pick scene using the latest target position
        // FIXME configurable reach
        const double reach = 100.0;
        targetGlobal = viewPosition + normalize(vecGlobal) * reach;
        pickList = globals->get_renderer()->pick(toOsg(viewPosition), toOsg(targetGlobal));
        if (!pickList.empty())
            targetGlobal = pickList.front().info.wgs84;

        // Update the cursor position, and update target if intentionally moved
        bool commit = motion || forcePick || _recenterPending;
        setTargetGlobal(targetGlobal, commit);
    }

    _cue->update();

    return pickList;
}

void FGMouseCursor3D::updateModel()
{
    // Get model switch node
    auto* sw = dynamic_cast<osg::Switch*>(getChild(0));
    assert(sw);

    FGMouseCursor::Cursor cursor = _cursor;
    if (!_visible)
        cursor = FGMouseCursor::CURSOR_NONE;

    int modelId = -1;
    if (cursor != FGMouseCursor::CURSOR_NONE) {
        if (cursor < _modelMapping.size())
            modelId = _modelMapping[cursor];
        // fall back to arrow, if that exists
        if (modelId < 0) {
            cursor = FGMouseCursor::CURSOR_ARROW;
            if (cursor < _modelMapping.size())
                modelId = _modelMapping[cursor];
        }
        // fall back to first modelId that does exist
        if (modelId < 0 && sw->getNumChildren() > 0)
            modelId = 0;
    }

    // Switch to chosen model
    if (_curModelId != modelId) {
        if (_curModelId >= 0)
            sw->setValue(_curModelId, false);
        _curModelId = modelId;
        if (_curModelId >= 0) {
            sw->setValue(_curModelId, true);
            _cue->showCue();
        } else {
            _cue->hideCue();
        }
    }
}
