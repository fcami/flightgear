// SPDX-FileCopyrightText: 2008 Tim Moore
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "WindowBuilder.hxx"
#include "WindowSystemAdapter.hxx"
#include <Main/fg_props.hxx>

#include <GUI/MessageBox.hxx>

#if defined(SG_MAC)
    #include <osgViewer/api/Cocoa/GraphicsWindowCocoa>
#endif

using namespace std;
using namespace osg;

// forwarding proxy from QtLauncher.cxx to avoid weird double-gl.h include
// errors from MSVC
void fgqt_setPoseAsStandaloneApp(bool b)
{
    flightgear::WindowBuilder::setPoseAsStandaloneApp(b);
}

namespace {

/// Helper function to set a value only if a given property exists.
/// Returns 1 if the value was modified, 0 if not.
template <typename T>
inline int setFromProperty(T& place, const SGPropertyNode* node, const std::string& name)
{
    const SGPropertyNode* prop = node->getNode(name);
    if (prop) {
        place = prop->getValue<T>();
        return 1;
    }
    return 0;
}

/// Helper function to create a graphics context for a given OpenGL version.
GraphicsContext* attemptToCreateGraphicsContext(const GraphicsContext::Traits* traits,
                                                const std::string& contextVersion,
                                                unsigned int profileMask)
{
    // We copy the traits object locally here because it gets deleted if
    // context creation is unsuccessful.
    ref_ptr<GraphicsContext::Traits> copy =
        new GraphicsContext::Traits(*traits);
    copy->glContextVersion = contextVersion;
    copy->glContextProfileMask = profileMask;
    return GraphicsContext::createGraphicsContext(copy);
}

} // anonymous namespace

namespace flightgear {

ref_ptr<WindowBuilder> WindowBuilder::windowBuilder;

// default to true (historical behaviour), we will clear the flag if
// we run another GUI.
bool WindowBuilder::poseAsStandaloneApp = true;

void WindowBuilder::initWindowBuilder()
{
    windowBuilder = new WindowBuilder();
}

WindowBuilder::WindowBuilder()
{
    makeDefaultTraits();
}

void WindowBuilder::makeDefaultTraits()
{
    GraphicsContext::WindowingSystemInterface* wsi
        = osg::GraphicsContext::getWindowingSystemInterface();
#if defined(HAVE_QT)
    if (usingQtGraphicsWindow) {
        // use the correct WSI for OpenSceneGraph >= 3.6
        wsi = osg::GraphicsContext::getWindowingSystemInterface("FlightGearQt5");
    }
#endif

    defaultTraits = new osg::GraphicsContext::Traits;

    auto traits = defaultTraits.get();
    traits->readDISPLAY();
    traits->setUndefinedScreenDetailsToDefaultScreen();
    traits->vsync = fgGetBool("/sim/rendering/vsync-enable", traits->vsync);
    traits->doubleBuffer = true;
    traits->mipMapGeneration = true;
    // Request a stencil buffer because paths on the Canvas desktop require it
    traits->stencil = 8;

    // TODO: Should be configurable by the Compositor on a per-window basis
    // traits->red = traits->green = traits->blue = cbits;
    // traits->depth = zbits;
    // traits->sampleBuffers = fgGetInt("/sim/rendering/multi-sample-buffers", traits->sampleBuffers);
    // traits->samples = fgGetInt("/sim/rendering/multi-samples", traits->samples);

    const bool wantFullscreen = fgGetBool("/sim/startup/fullscreen");
    unsigned screenwidth = 0;
    unsigned screenheight = 0;
    // this is a deprecated method, should be screen-aware.
    wsi->getScreenResolution(*traits, screenwidth, screenheight);

    // handle fullscreen manually
    traits->windowDecoration = !wantFullscreen;
    if (!traits->windowDecoration) {
        // fullscreen
        traits->supportsResize = false;
        traits->width = screenwidth;
        traits->height = screenheight;
        SG_LOG(SG_VIEW,SG_DEBUG,"Using full screen size for window: " << screenwidth << " x " << screenheight);
    } else {
        // window
        int w = fgGetInt("/sim/startup/xsize");
        int h = fgGetInt("/sim/startup/ysize");
        traits->supportsResize = true;
        traits->width = w;
        traits->height = h;
        if ((w>0)&&(h>0))
        {
            traits->x = ((unsigned)w>screenwidth) ? 0 : (screenwidth-w)/3;
            traits->y = ((unsigned)h>screenheight) ? 0 : (screenheight-h)/3;
        }
        SG_LOG(SG_VIEW,SG_DEBUG,"Using initial window size: " << w << " x " << h);
    }
}

void WindowBuilder::setFullscreenTraits(const SGPropertyNode* winNode, GraphicsContext::Traits* traits)
{
    const SGPropertyNode* orrNode = winNode->getNode("overrideRedirect");
    bool overrideRedirect = orrNode && orrNode->getBoolValue();
    traits->overrideRedirect = overrideRedirect;

    traits->windowDecoration = false;

    unsigned int width = 0;
    unsigned int height = 0;
    auto wsi = osg::GraphicsContext::getWindowingSystemInterface();
    wsi->getScreenResolution(*traits, width, height);
    traits->width = width;
    traits->height = height;
    traits->supportsResize = false;
    traits->x = 0;
    traits->y = 0;
}

bool WindowBuilder::setWindowedTraits(const SGPropertyNode* winNode, GraphicsContext::Traits* traits)
{
    bool customTraits = false;
    int resizable = 0;
    const SGPropertyNode* fullscreenNode = winNode->getNode("fullscreen");
    if (fullscreenNode && !fullscreenNode->getBoolValue())
    {
        traits->windowDecoration = true;
        resizable = 1;
    }
    resizable |= setFromProperty(traits->windowDecoration, winNode, "decoration");
    resizable |= setFromProperty(traits->width, winNode, "width");
    resizable |= setFromProperty(traits->height, winNode, "height");
    if (resizable) {
        traits->supportsResize = true;
        customTraits = true;
    }

    return customTraits;
}

void WindowBuilder::setMacPoseAsStandaloneApp(GraphicsContext::Traits* traits) const
{
#if defined(SG_MAC)
    // this logic is unnecessary if using a Qt window, since everything
    // plays together nicely
    int flags = osgViewer::GraphicsWindowCocoa::WindowData::CheckForEvents;

    // avoid both QApplication and OSG::CocoaViewer doing single-application
    // init (Apple menu, making front process, etc)
    if (poseAsStandaloneApp) {
        flags |= osgViewer::GraphicsWindowCocoa::WindowData::PoseAsStandaloneApp;
    }
    traits->inheritedWindowData = new osgViewer::GraphicsWindowCocoa::WindowData(flags);
#endif
}

GraphicsWindow* WindowBuilder::buildWindow(const SGPropertyNode* winNode)
{
    WindowSystemAdapter* wsa = WindowSystemAdapter::getWSA();

    string windowName;
    if (winNode->hasChild("window-name")) {
        windowName = winNode->getStringValue("window-name");
    } else if (winNode->hasChild("name")) {
        windowName = winNode->getStringValue("name");
    } else {
        SG_LOG(SG_VIEW, SG_WARN, "WindowBuilder::buildWindow: Window needs a name");
        return nullptr;
    }

    // look for an existing window and return that
    GraphicsWindow* result = wsa->findWindow(windowName);
    if (result) {
        return result;
    }

    // There is no existing window with this name, so create it from scratch.
    // Copy the default traits and modify them according to the window props.
    ref_ptr<GraphicsContext::Traits> traits =
        new GraphicsContext::Traits(*defaultTraits);

    // Attempt to share context with the window that was created first
    if (!wsa->windows.empty())
        traits->sharedContext = wsa->windows.front()->gc;

    [[maybe_unused]] int traitsSet = 0;
    traitsSet |= setFromProperty(traits->hostName, winNode, "host-name");
    traitsSet |= setFromProperty(traits->displayNum, winNode, "display");
    traitsSet |= setFromProperty(traits->screenNum, winNode, "screen");

    const SGPropertyNode* fullscreenNode = winNode->getNode("fullscreen");
    if (fullscreenNode && fullscreenNode->getBoolValue()) {
        setFullscreenTraits(winNode, traits);
        traitsSet = 1;
    } else {
        traitsSet |= setWindowedTraits(winNode, traits);
    }
    traitsSet |= setFromProperty(traits->x, winNode, "x");
    traitsSet |= setFromProperty(traits->y, winNode, "y");

    // The window title matches the internal window name by default
    traits->windowName = windowName;
    traitsSet |= setFromProperty(traits->windowName, winNode, "title");

    setMacPoseAsStandaloneApp(traits);

    // Create a graphics context for this window.
    // This is where we choose which OpenGL version to use.
    // We also set the #version string for shaders in the display settings based
    // on the chosen OpenGL version.
    auto display_settings = osg::DisplaySettings::instance();
    GraphicsContext* gc = nullptr;

#if !defined(SG_MAC)
    // Attempt to create an OpenGL 4.3 core profile context first if we are not
    // on MacOS (max version there is 4.1). We can optionally take advantage of
    // 4.3 features like compute shaders.
    display_settings->setValue("FG_GLSL_VERSION", "#version 430 core");
    gc = attemptToCreateGraphicsContext(traits, "4.3", 0x1);
#endif

    if (!gc) {
        // 4.3 is unsupported, so try with 4.1. This version is required, i.e.
        // we crash if we can't successfully create an OpenGL context.
        display_settings->setValue("FG_GLSL_VERSION", "#version 410 core");
        gc = attemptToCreateGraphicsContext(traits, "4.1", 0x1);
        if (!gc) {
            flightgear::fatalMessageBoxThenExit(
                "Unable to create OpenGL 4.1 core profile context",
                "FlightGear was unable to create a window supporting 3D rendering. "
                "This is normally due to outdated graphics drivers, please check if updates are available. ",
                "Depending on your OS and graphics chipset, updates might come from AMD, nVidia or Intel.");
            return nullptr; // unreachable anyway
        }
    }

    // Cache the newly created window using the internal name
    // (because traits->windowName can be custom).
    result = WindowSystemAdapter::getWSA()->registerWindow(gc, windowName);

    return result;
}

void WindowBuilder::setPoseAsStandaloneApp(bool b)
{
    poseAsStandaloneApp = b;
}

} // of namespace flightgear
