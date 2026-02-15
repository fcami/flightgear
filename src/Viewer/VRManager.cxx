// SPDX-FileCopyrightText: 2021 James Hogan <james@albanarts.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VRManager.hxx"
#include "WindowBuilder.hxx"
#include "renderer.hxx"

#include <osgXR/Settings>

#include <simgear/scene/util/RenderConstants.hxx>
#include <simgear/scene/viewer/Compositor.hxx>
#include <simgear/scene/viewer/CompositorPass.hxx>
#include <simgear/structure/commands.hxx>

#include <Main/fg_props.hxx>
#include <Main/globals.hxx>

namespace flightgear
{
using namespace simgear;
using namespace compositor;

// Unfortunately, this can't be scoped inside VRManager::instance().
// If its initialisation completes after main() calls atexit(fgExitCleanup),
// then its destruction should take place before fgExitCleanup() is called.
static osg::ref_ptr<VRManager> managerInstance;

VRManager::VRManager()
    : _reloadCompositorCallback(new ReloadCompositorCallback(this)),
      _headSpace(new osgXR::RefSpaceView(this,
                                         // Offset head space back 10cm from view space
                                         osgXR::Pose(osg::Quat(), osg::Vec3f(0.0f, 0.0f, -0.1f)))),
      _propXrLayersValidation("/sim/vr/openxr/layers/validation"),
      _propXrExtensionsDepthInfo("/sim/vr/openxr/extensions/depth-info"),
      _propXrExtensionsVisibilityMask("/sim/vr/openxr/extensions/visibility-mask"),
      _propXrRuntimeName("/sim/vr/openxr/runtime/name"),
      _propXrSystemName("/sim/vr/openxr/system/name"),
      _propXrSystemUserPresence("/sim/vr/openxr/system/supports-user-presence"),
      _propStateString("/sim/vr/state-string"),
      _propPresent("/sim/vr/present"),
      _propRunning("/sim/vr/running"),
      _propUserPresent("/sim/vr/user-present"),
      _propEnabled("/sim/vr/enabled"),
      _propDepthInfo("/sim/vr/depth-info"),
      _propVisibilityMask("/sim/vr/visibility-mask"),
      _propValidationLayer("/sim/vr/validation-layer"),
      _propMode("/sim/vr/mode"),
      _propSwapchainMode("/sim/vr/swapchain-mode"),
      _propMirrorEnabled("/sim/vr/mirror-enabled"),
      _propMirrorMode("/sim/vr/mirror-mode"),
      _listenerEnabled(this, &osgXR::Manager::setEnabled),
      _listenerDepthInfo(this, &VRManager::setDepthInfo),
      _listenerVisibilityMask(this, &VRManager::setVisibilityMask),
      _listenerValidationLayer(this, &VRManager::setValidationLayer),
      _listenerMode(this, &VRManager::setVRMode),
      _listenerSwapchainMode(this, &VRManager::setSwapchainMode),
      _listenerMirrorMode(this, &VRManager::setMirrorMode)
{
    uint32_t fgVersion = (FLIGHTGEAR_MAJOR_VERSION << 16 |
                          FLIGHTGEAR_MINOR_VERSION << 8 |
                          FLIGHTGEAR_PATCH_VERSION);
    _settings->setApp("FlightGear", fgVersion);
    _settings->preferEnvBlendMode(osgXR::Settings::BLEND_MODE_OPAQUE);

    // Inform osgXR what node masks to use
    setVisibilityMaskNodeMasks(simgear::NodeMask::LEFT_BIT,
                               simgear::NodeMask::RIGHT_BIT);

    // Hook into viewer, but don't enable VR just yet
    osgViewer::View *view = globals->get_renderer()->getView();
    if (view) {
        setViewer(globals->get_renderer()->getViewerBase());
        view->apply(this);
    }

    syncReadOnlyProperties();
    _propUserPresent = false;

    _propEnabled.node(true)->addChangeListener(&_listenerEnabled, true);
    _propDepthInfo.node(true)->addChangeListener(&_listenerDepthInfo, true);
    _propVisibilityMask.node(true)->addChangeListener(&_listenerVisibilityMask, true);
    _propValidationLayer.node(true)->addChangeListener(&_listenerValidationLayer, true);
    _propMode.node(true)->addChangeListener(&_listenerMode, true);
    _propSwapchainMode.node(true)->addChangeListener(&_listenerSwapchainMode, true);
    _propMirrorMode.node(true)->addChangeListener(&_listenerMirrorMode, true);

    // No need for a change listener, but it should still be resolvable
    _propMirrorEnabled.node(true);

    globals->get_commands()->addCommand("vr-recenter", this, &VRManager::cmdRecenter);

    // Determine what multiview support the default compositor implements.
    std::string compositorPath = fgGetString("/sim/rendering/default-compositor",
                                              "Compositor/default");
    SGPropertyNode_ptr compositorProps = Compositor::loadPropertyList(compositorPath);
    if (compositorProps.valid()) {
        _settings->setViewAlignmentMask(compositorProps->getIntValue("multiview/view-align-mask", 0));

        _settings->allowVRMode(osgXR::Settings::VRMODE_SLAVE_CAMERAS);
        if (compositorProps->getBoolValue("multiview/sceneview", false))
            _settings->allowVRMode(osgXR::Settings::VRMODE_SCENE_VIEW);

        _settings->allowSwapchainMode(osgXR::Settings::SWAPCHAIN_MULTIPLE);
        if (compositorProps->getBoolValue("multiview/intermediates-tiled", false))
            _settings->allowSwapchainMode(osgXR::Settings::SWAPCHAIN_SINGLE);
    }
}

VRManager::~VRManager()
{
    globals->get_commands()->removeCommand("vr-recenter");
}

VRManager* VRManager::instance(bool destroy)
{
    static bool initialised = false;
    if (destroy) {
        if (initialised) {
            managerInstance->destroyAndWait();
            globals->get_renderer()->getViewerBase()->setRealizeOperation(nullptr);
            globals->get_renderer()->getView()->apply(nullptr);
        }
        managerInstance = nullptr;
        initialised = false;
        return nullptr;
    }
    if (!initialised) {
        managerInstance = new VRManager;
        initialised = true;
    }
    return managerInstance;
}

void VRManager::reset()
{
    auto composite_viewer = globals->get_renderer()->getCompositeViewer();

    // Force retrigger of the CompositeViewer realize callback.
    // We reuse the graphics context, so it will already be realized. This makes
    // the CompositeViewer become realized without its realize operation
    // callback being called, preventing osgXR from being reinitialised.

    auto* viewerRealizeOp = composite_viewer->getRealizeOperation();
    if (viewerRealizeOp) {
        osgViewer::ViewerBase::Contexts contexts;
        composite_viewer->getContexts(contexts);
        for (auto citr = contexts.begin(); citr != contexts.end(); ++citr) {
            osg::GraphicsContext* gc = *citr;

            if (gc->valid()) {
                gc->makeCurrent();
                (*viewerRealizeOp)(gc);
                gc->releaseContext();
            }
        }
    }
}

void VRManager::syncProperties()
{
    // If the state has changed, properties may need synchronising
    if (checkAndResetStateChanged()) {
        syncReadOnlyProperties();
        syncSettingProperties();
    }
}

void VRManager::syncReadOnlyProperties()
{
    _propXrLayersValidation = hasValidationLayer();
    _propXrExtensionsDepthInfo = hasDepthInfoExtension();
    _propXrExtensionsVisibilityMask = hasVisibilityMaskExtension();
    _propXrRuntimeName = getRuntimeName();
    _propXrSystemName = getSystemName();
    _propXrSystemUserPresence = supportsUserPresence();

    _propStateString = getStateString();
    _propPresent = getPresent();
    _propRunning = isRunning();
}

void VRManager::syncSettingProperties()
{
    bool enabled = getEnabled();
    if (_propEnabled != enabled)
        _propEnabled = enabled;
}

bool VRManager::getUseMirror() const
{
    return _propMirrorEnabled && isRunning();
}

void VRManager::setValidationLayer(bool validationLayer)
{
    _settings->setValidationLayer(validationLayer);
    syncSettings();
}

void VRManager::setDepthInfo(bool depthInfo)
{
    _settings->setDepthInfo(depthInfo);
    syncSettings();
}

void VRManager::setVisibilityMask(bool visibilityMask)
{
    _settings->setVisibilityMask(visibilityMask);
    syncSettings();
}

void VRManager::setVRMode(const std::string& mode)
{
    osgXR::Settings::VRMode vrMode = osgXR::Settings::VRMODE_AUTOMATIC;

    if (mode == "AUTOMATIC") {
        vrMode = osgXR::Settings::VRMODE_AUTOMATIC;
    } else if (mode == "SLAVE_CAMERAS") {
        vrMode = osgXR::Settings::VRMODE_SLAVE_CAMERAS;
    } else if (mode == "SCENE_VIEW") {
        vrMode = osgXR::Settings::VRMODE_SCENE_VIEW;
    }

    _settings->setVRMode(vrMode);
    syncSettings();
}

void VRManager::setSwapchainMode(const std::string& mode)
{
    osgXR::Settings::SwapchainMode swapchainMode = osgXR::Settings::SWAPCHAIN_AUTOMATIC;

    if (mode == "AUTOMATIC") {
        swapchainMode = osgXR::Settings::SWAPCHAIN_AUTOMATIC;
    } else if (mode == "MULTIPLE") {
        swapchainMode = osgXR::Settings::SWAPCHAIN_MULTIPLE;
    } else if (mode == "SINGLE") {
        swapchainMode = osgXR::Settings::SWAPCHAIN_SINGLE;
    }

    _settings->setSwapchainMode(swapchainMode);
    syncSettings();
}

void VRManager::setMirrorMode(const std::string& mode)
{
    osgXR::MirrorSettings::MirrorMode mirrorMode = osgXR::MirrorSettings::MIRROR_AUTOMATIC;
    int viewIndex = -1;

    if (mode == "AUTOMATIC") {
        mirrorMode = osgXR::MirrorSettings::MIRROR_AUTOMATIC;
    } else if (mode == "NONE") {
        mirrorMode = osgXR::MirrorSettings::MIRROR_NONE;
    } else if (mode == "LEFT") {
        mirrorMode = osgXR::MirrorSettings::MIRROR_SINGLE;
        viewIndex = 0;
    } else if (mode == "RIGHT") {
        mirrorMode = osgXR::MirrorSettings::MIRROR_SINGLE;
        viewIndex = 1;
    } else if (mode == "LEFT_RIGHT") {
        mirrorMode = osgXR::MirrorSettings::MIRROR_LEFT_RIGHT;
    }

    _settings->getMirrorSettings().setMirror(mirrorMode, viewIndex);
}

void VRManager::update()
{
    osgXR::Manager::update();
    syncProperties();
}

void VRManager::doCreateView(osgXR::View *xrView)
{
    // Restarted in osgXR::Manager::update()
    _viewer->stopThreading();

    // Construct a property tree for the camera
    SGPropertyNode_ptr camNode = new SGPropertyNode;
    setValue(camNode->getNode("window/name", true),
             flightgear::DEFAULT_WINDOW_NAME);
    setValue(camNode->getNode("viewport/width", true), (int)xrView->getMVRWidth());
    setValue(camNode->getNode("viewport/height", true), (int)xrView->getMVRHeight());
    setValue(camNode->getNode("mvr-views", true), (int)xrView->getMVRViews());
    setValue(camNode->getNode("mvr-view-id-global", true), xrView->getMVRViewIdGlobalStr());
    setValue(camNode->getNode("mvr-view-id-vert", true), xrView->getMVRViewIdStr(GL_VERTEX_SHADER));
    setValue(camNode->getNode("mvr-view-id-geom", true), xrView->getMVRViewIdStr(GL_GEOMETRY_SHADER));
    setValue(camNode->getNode("mvr-view-id-frag", true), xrView->getMVRViewIdStr(GL_FRAGMENT_SHADER));
    setValue(camNode->getNode("mvr-cells", true), (int)xrView->getMVRCells());

    // Build a camera
    CameraGroup *cgroup = CameraGroup::getDefault();
    CameraInfo *info = cgroup->buildCamera(camNode);

    // Notify osgXR about the new compositor's scene slave cameras
    if (info) {
        _camInfos[xrView] = info;
        _xrViews[info] = xrView;
        info->reloadCompositorCallback = _reloadCompositorCallback;

        postReloadCompositor(cgroup, info);
    }

    // Get notified of subview changes
    xrView->setCallback(new ViewCallback(this));
}

void VRManager::doDestroyView(osgXR::View *xrView)
{
    // Restarted in osgXR::Manager::update()
    _viewer->stopThreading();

    CameraGroup *cgroup = CameraGroup::getDefault();
    auto it = _camInfos.find(xrView);
    if (it != _camInfos.end()) {
        osg::ref_ptr<CameraInfo> info = (*it).second;
        _camInfos.erase(it);

        auto it2 = _xrViews.find(info.get());
        if (it2 != _xrViews.end())
            _xrViews.erase(it2);

        cgroup->removeCamera(info.get());
    }
}

void VRManager::onRunning()
{
    // Reload compositors to trigger switch to mirror of VR
    CameraGroup *cgroup = CameraGroup::getDefault();
    reloadCompositors(cgroup);
}

void VRManager::onStopped()
{
    _propUserPresent = false;

    // As long as we're not in the process of destroying FlightGear, reload
    // compositors to trigger switch away from mirror of VR
    if (!isDestroying())
    {
        CameraGroup *cgroup = CameraGroup::getDefault();
        reloadCompositors(cgroup);
    }
}

void VRManager::onUserPresence(bool userPresent)
{
    _propUserPresent = userPresent;
}

static osgXR::View::Flags getPassVRFlags(const simgear::compositor::Pass *pass)
{
    bool isScene = (pass->type == "scene");
    bool isMultiviewQuad = (pass->type == "quad" && pass->multiview == "true");
    bool isWidthScaled = (pass->viewport_width_scale != 0.0f);
    bool isHeightScaled = (pass->viewport_height_scale != 0.0f);
    // NOTE: isToFb may change after osgXR has made changes.
    // Whether getPassVRFlags() returns 0 must not change as a result.
    bool isToFb = (pass->camera->getRenderTargetImplementation() == osg::Camera::FRAME_BUFFER);

    osgXR::View::Flags flags = osgXR::View::CAM_NO_BITS;

    // If camera renders to the frame buffer, redirect to XR.
    if ((isScene || isMultiviewQuad) && isToFb)
        flags |= osgXR::View::CAM_TOXR_BIT;

    if (isScene && (isToFb || (isWidthScaled && isHeightScaled))) {
        // If scene is rendered to a scaled viewport, perform multiview scene
        // rendering with shading.
        flags |= osgXR::View::CAM_MVR_SCENE_BIT;
        flags |= osgXR::View::CAM_MVR_SHADING_BIT;
    } else if (isMultiviewQuad) {
        // If multiview quad is rendered, perform multiview shading
        flags |= osgXR::View::CAM_MVR_SHADING_BIT;

        if (!(flags & osgXR::View::CAM_TOXR_BIT)) {
            // Fixed size MVR results in identically sized viewports
            if (!isWidthScaled)
                flags |= osgXR::View::CAM_MVR_FIXED_WIDTH_BIT;
            if (!isHeightScaled)
                flags |= osgXR::View::CAM_MVR_FIXED_HEIGHT_BIT;
        }
    }

    return flags;
}

void VRManager::preReloadCompositor(CameraGroup *cgroup, CameraInfo *info)
{
    osgXR::View *xrView = _xrViews[info];

    auto& passes = info->compositor->getPassList();
    for (auto& pass: passes) {
        // osgXR may change camera's render target implementation (isToFb), but
        // it shouldn't change whether flags == 0
        auto flags = getPassVRFlags(pass);
        if (flags)
            xrView->removeSlave(pass->camera);
    }
}

void VRManager::postReloadCompositor(CameraGroup *cgroup, CameraInfo *info)
{
    osgXR::View *xrView = _xrViews[info];

    auto& passes = info->compositor->getPassList();
    for (auto& pass: passes) {
        auto flags = getPassVRFlags(pass);
        if (flags)
            xrView->addSlave(pass->camera, flags);
    }
}

void VRManager::updateSubView(osgXR::View *view, unsigned int subviewIndex,
                              const osgXR::View::SubView &subview)
{
    auto it = _camInfos.find(view);
    if (it == _camInfos.end())
        return;

    osg::ref_ptr<CameraInfo> info = (*it).second;

    osg::Matrix viewMatrix = subview.getViewMatrix();
    osg::Matrix projMatrix = subview.getProjectionMatrix();

    // see CameraGroup::update()
    viewMatrix = info->viewOffset * viewMatrix;
    if ((info->flags & CameraInfo::VIEW_ABSOLUTE) == 0) {
        auto *masterCam = CameraGroup::getDefault()->getView()->getCamera();
        viewMatrix = masterCam->getViewMatrix() * viewMatrix;
    }

    auto vp = subview.getViewport();
    info->compositor->updateSubView(subviewIndex, viewMatrix, projMatrix,
                                    osg::Vec4(vp.x, vp.y, vp.w, vp.h));
}

bool VRManager::cmdRecenter(const SGPropertyNode* arg, SGPropertyNode* root)
{
    SG_UNUSED(arg);
    SG_UNUSED(root);
    return recenter();
}

}
