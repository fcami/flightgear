// Viewer.hxx -- alternative flightgear viewer application
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Mathias Froehlich

#ifndef _FGVIEWER_VIEWER_HXX
#define _FGVIEWER_VIEWER_HXX

#include <osgViewer/Viewer>

#include <simgear/math/SGMath.hxx>
#include <simgear/scene/util/SGReaderWriterOptions.hxx>
#include <simgear/timing/timestamp.hxx>
#include <simgear/props/props.hxx>

#include "Drawable.hxx"
#include "Frustum.hxx"
#include "Renderer.hxx"
#include "SlaveCamera.hxx"
#include "ArgumentParser.hxx"

namespace fgviewer  {

class Viewer : public osgViewer::Viewer {
public:
    Viewer(ArgumentParser& arguments);
    virtual ~Viewer();

    bool readCameraConfig(const SGPropertyNode& viewerNode);
    void setupDefaultCameraConfigIfUnset();
    /// Short circuit osg config files.
    virtual bool readConfiguration(const std::string& filename);

    /// Callback class that cares for the camera and drawable setup
    void setRenderer(Renderer* renderer);
    Renderer* getRenderer();

    /// Access and create drawables
    Drawable* getOrCreateDrawable(const std::string& name);
    Drawable* getDrawable(const std::string& name);
    unsigned getDrawableIndex(const std::string& name);
    Drawable* getDrawable(unsigned index);
    unsigned getNumDrawables() const;

    /// Access and create slave cameras
    SlaveCamera* getOrCreateSlaveCamera(const std::string& name);
    SlaveCamera* getSlaveCamera(const std::string& name);
    unsigned getSlaveCameraIndex(const std::string& name);
    SlaveCamera* getSlaveCamera(unsigned index);
    unsigned getNumSlaveCameras() const;

    /// Realize the contexts and attach the cameras there
    virtual void realize();
    bool realizeDrawables();
    bool realizeSlaveCameras();

    /// exec methods
    virtual void advance(double simTime);
    virtual void updateTraversal();
    bool updateSlaveCameras();

    /// Store this per viewer instead of global.
    void setReaderWriterOptions(simgear::SGReaderWriterOptions* readerWriterOptions);
    simgear::SGReaderWriterOptions* getReaderWriterOptions();

    /// Puts the scene data under the renderer.
    /// Replaces the whole renderer independent scene.
    virtual void setSceneData(osg::Node* node);

    /// Adds the scene node to the global scene
    /// Use this to add something to the displayed scene.
    void insertSceneData(osg::Node* node);
    bool insertSceneData(const std::string& fileName, const osgDB::Options* options = 0);
    /// Return the scene data group.
    osg::Group* getSceneDataGroup();

    /// Traverse the scenegraph and throw out all child nodes that can be loaded again.
    void purgeLevelOfDetailNodes();

    /// Return a default screen identifier. Under UNIX the default DISPLAY environment variable.
    static osg::GraphicsContext::ScreenIdentifier getDefaultScreenIdentifier();
    /// Interpret the given display. Is mergend with the default screen identifier.
    static osg::GraphicsContext::ScreenIdentifier getScreenIdentifier(const std::string& display);
    /// Return screen settings, mostly the resolution of the given screen.
    osg::GraphicsContext::ScreenSettings getScreenSettings(const osg::GraphicsContext::ScreenIdentifier& screenIdentifier);
    /// Return traits struct for the given screen identifier. The size and position is already set up for a fullscreen window.
    osg::GraphicsContext::Traits* getTraits(const osg::GraphicsContext::ScreenIdentifier& screenIdentifier);
    /// Helper to create an new graphics context from traits.
    osg::GraphicsContext* createGraphicsContext(osg::GraphicsContext::Traits* traits);

private:
    Viewer(const Viewer&);
    Viewer& operator=(const Viewer&);

    /// Unload all lod's
    class _PurgeLevelOfDetailNodesVisitor;
#ifdef __linux__
    /// Under linux make sure that the screen saver does not jump in
    class _ResetScreenSaverSwapCallback;
#endif

    /// The renderer used to setup the higher level camera/scenegraph structure
    SGSharedPtr<Renderer> _renderer;

    /// The drawables managed by this viewer.
    typedef std::vector<SGSharedPtr<Drawable> > DrawableVector;
    DrawableVector _drawableVector;

    /// The slave cameras for this viewer.
    /// Since we support more complex renderers, we can not only use osg::View::Slave.
    typedef std::vector<SGSharedPtr<SlaveCamera> > SlaveCameraVector;
    SlaveCameraVector _slaveCameraVector;

    /// The top level options struct
    osg::ref_ptr<simgear::SGReaderWriterOptions> _readerWriterOptions;

    /// The top level scenegraph structure that is used for drawing
    osg::ref_ptr<osg::Group> _sceneDataGroup;

    /// Stores the time increment for each frame.
    /// If zero, the time advance is done to the current real time.
    SGTimeStamp _timeIncrement;
    /// The current simulation time of the viewer
    SGTimeStamp _simTime;
};

} // namespace fgviewer

#endif
