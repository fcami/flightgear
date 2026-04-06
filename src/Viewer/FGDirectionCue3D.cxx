// SPDX-FileCopyrightText: 2025 James Hogan <james@albanarts.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Implements 3D direction cue visible in the scene.
 */

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

FGDirectionCue3D::FGDirectionCue3D(osg::Node* target)
    : _target(target)
{
    setName("3D direction cue for " + target->getName());

    // Set up properties and usable defaults
    SGPropertyNode_ptr cue3DNode = fgGetNode("/sim/vr/config/cursors/cue", true);

    _propVisibilityAngleDeg = SGPropObjDouble(cue3DNode, "visibility-angle-deg");
    _propVisibilityAngleDeg.setDefault(30.0);

    _propAngleDeg = SGPropObjDouble(cue3DNode, "angle-deg");
    _propAngleDeg.setDefault(10.0);

    _propDistanceM = SGPropObjDouble(cue3DNode, "distance-m");
    _propDistanceM.setDefault(0.45);

    osg::Switch* sw = new osg::Switch;
    addChild(sw);

    // Read direction cue model from /sim/vr/config/cursors/cue
    std::string path = cue3DNode->getStringValue("model/path", "");
    simgear::ErrorReportContext ec("direction-cue-model", path);

    SGPath resolvedPath = globals->resolve_aircraft_path(path);
    if (resolvedPath.isNull()) {
        simgear::reportFailure(simgear::LoadFailure::NotFound,
                               simgear::ErrorCode::XMLModelLoad,
                               "Failed to find 3D direction cue model",
                               SGPath::fromUtf8(path));
        return;
    }

    osg::Node* node = simgear::SGModelLib::loadModel(resolvedPath.utf8Str(),
                                                     globals->get_props());
    if (node) {
        // And add the model node to the switch
        sw->addChild(node, false);
    }

    // Don't allow the cue to be picked.
    // Don't allow the aircraft to collide catastrophically with the cue!
    setNodeMask(~SG_NODEMASK_PICK_BIT & ~SG_NODEMASK_TERRAIN_BIT);
}

void FGDirectionCue3D::update()
{
    // We need the current view
    auto* view_mgr = globals->get_subsystem<FGViewMgr>();
    if (!view_mgr)
        return;
    auto* view = view_mgr->get_current_view();
    if (!view)
        return;

    // Target must still exist
    if (!_target.valid()) {
        _targetValid = false;
        updateModel();
        return;
    }

    // Get the world target position
    auto targetMats = _target->getWorldMatrices();
    if (targetMats.empty()) {
        _targetValid = false;
        updateModel();
        return;
    }
    SGVec3d targetGlobal = toSG(targetMats[0].getTrans());

    // Transform target into view space
    SGVec3d viewPosition = view->getViewPosition();
    SGQuatd viewOrientation = view->getViewOrientation();
    SGVec3d targetView = viewOrientation.transform(targetGlobal - viewPosition);
    SGVec3d targetViewNorm = normalize(targetView);

    // Configuration
    const double visibilityAngleRad = SGMiscd::deg2rad(_propVisibilityAngleDeg);
    const double cueAngleRad = SGMiscd::deg2rad(_propAngleDeg);
    const double cueDistM = _propDistanceM;

    // Is the target roughly in view?
    // View points in -Z direction
    if (-targetViewNorm.z() > cos(visibilityAngleRad)) {
        _targetValid = false;
        updateModel();
        return;
    }

    // Get aircraft pose
    auto* aircraft = globals->get_subsystem<FGAircraftModel>()->get3DModel();
    auto aircraftPosition = aircraft->getPosition();
    SGQuatd aircraftOrient = aircraft->getGlobalOrientation();

    // Update the cue position
    double targetDirection = atan2(targetViewNorm.x(), targetViewNorm.y());
    SGVec3d cuePosLocal = SGVec3d(0.0, cueDistM * sin(cueAngleRad),
                                  -cueDistM * cos(cueAngleRad));
    SGQuatd cueOrView = SGQuatd::fromAngleAxis(targetDirection, SGVec3d(0.0, 0.0, -1.0));
    SGVec3d cuePosView = cueOrView.backTransform(cuePosLocal);
    SGVec3d cuePosGlobal = viewPosition + viewOrientation.backTransform(cuePosView);
    SGQuatd cueOrGlobal = viewOrientation * cueOrView;
    SGVec3d cuePosAircraft = aircraftOrient.transform(cuePosGlobal - SGVec3d::fromGeod(aircraftPosition));
    SGQuatd cueOrAircraft = inverse(aircraftOrient) * cueOrGlobal;

    osg::Matrix mat;
    mat.setTrans(toOsg(cuePosAircraft));
    mat.setRotate(toOsg(cueOrAircraft));
    setMatrix(mat);

    // Add cue to scene graph under aircraft
    if (!getNumParents())
        aircraft->add(this);

    _targetValid = true;
    updateModel();
}

void FGDirectionCue3D::updateModel()
{
    // Get model switch node
    auto* sw = dynamic_cast<osg::Switch*>(getChild(0));
    assert(sw);

    // Make the direction cue model visible or not
    if (sw->getNumChildren() >= 1)
        sw->setValue(0, _visible && _targetValid);
}
