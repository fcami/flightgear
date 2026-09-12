// SPDX-FileCopyrightText: 2026 FlightGear Contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "OSWindowPlacement.hxx"

#include <Main/globals.hxx>
#include <Main/fg_props.hxx>
#include <Viewer/renderer.hxx>

#include <simgear/canvas/Canvas.hxx>
#include <simgear/canvas/events/MouseEvent.hxx>
#include <simgear/debug/logstream.hxx>

#include <osg/Geometry>
#include <osgViewer/CompositeViewer>
#include <osgViewer/View>
#include <osgViewer/GraphicsWindow>

namespace sc = simgear::canvas;

namespace canvas {

OSWindowPlacement::OSWindowPlacement(SGPropertyNode* node, sc::CanvasPtr canvas)
    : Placement(node), _canvas(canvas)
{
    FGRenderer* renderer = globals->get_renderer();
    auto* cv = dynamic_cast<osgViewer::CompositeViewer*>(renderer->getViewerBase());
    if (!cv) return;

    int w = node->getIntValue("width", 512);
    int h = node->getIntValue("height", 512);

    _view = new osgViewer::View;
    _view->setName("CanvasPopout");
    _view->setFrameStamp(cv->getFrameStamp());
    _view->setUpViewInWindow(
        node->getIntValue("x", 100), node->getIntValue("y", 100),
        w, h, node->getIntValue("screen", 0));

    auto* gw = dynamic_cast<osgViewer::GraphicsWindow*>(
        _view->getCamera()->getGraphicsContext());
    if (gw) gw->setWindowName(node->getStringValue("title", "Canvas"));

    osg::Camera* cam = _view->getCamera();
    cam->setProjectionMatrix(osg::Matrix::ortho2D(0, w, 0, h));
    cam->setViewMatrix(osg::Matrix::identity());
    cam->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    cam->setClearColor(osg::Vec4(0, 0, 0, 1));
    cam->setClearMask(GL_COLOR_BUFFER_BIT);

    // Force canvas to render every frame
    canvas->getProps()->setBoolValue("render-always", true);

    // Canvas FBO texture can't cross to this unshared context.
    // Use glGetTexImage to read the texture into an osg::Image each frame.
    osg::Texture2D* srcTex = canvas->getTexture();
    int cw = srcTex ? srcTex->getTextureWidth() : 256;
    int ch = srcTex ? srcTex->getTextureHeight() : 256;

    _image = new osg::Image;
    _image->allocateImage(cw, ch, 1, GL_RGBA, GL_UNSIGNED_BYTE);

    // Post-draw callback: reads canvas texture via glGetTexImage
    // This reads from the texture object directly, not from the current FBO.
    struct ReadbackCallback : public osg::Camera::DrawCallback {
        osg::ref_ptr<osg::Image> image;
        osg::ref_ptr<osg::Texture2D> srcTexture;
        unsigned int contextID;
        ReadbackCallback(osg::Image* img, osg::Texture2D* tex, unsigned int ctx)
            : image(img), srcTexture(tex), contextID(ctx) {}
        void operator()(osg::RenderInfo&) const override {
            if (!image->valid() || !srcTexture) return;
            auto* texObj = srcTexture->getTextureObject(contextID);
            if (!texObj) return;
            int w = image->s(), h = image->t();
            (void)w; (void)h;
            texObj->bind();
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->data());
            image->dirty();
        }
    };

    osg::Camera* canvasCam = canvas->getCamera();
    unsigned int contextID = 0;
    if (canvasCam && canvasCam->getGraphicsContext())
        contextID = canvasCam->getGraphicsContext()->getState()->getContextID();

    if (canvasCam) {
        _readbackCb = new ReadbackCallback(_image, srcTex, contextID);
        canvasCam->setPostDrawCallback(_readbackCb);
    }

    _localTex = new osg::Texture2D;
    _localTex->setImage(_image);
    _localTex->setResizeNonPowerOfTwoHint(false);
    _localTex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
    _localTex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    _localTex->setDataVariance(osg::Object::DYNAMIC);

    osg::Geode* quad = new osg::Geode;
    quad->setCullingActive(false);
    // Tex coords: l, b, r, t
    // Canvas renders Y-down into FBO. glGetTexImage reads texture as-is.
    // Need to flip vertically: swap top and bottom tex coords.
    osg::Geometry* geom = osg::createTexturedQuadGeometry(
        osg::Vec3(0, 0, 0), osg::Vec3(w, 0, 0), osg::Vec3(0, h, 0),
        0.0f, 0.0f, 1.0f, 1.0f);
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(false);
    quad->addDrawable(geom);
    osg::StateSet* ss = quad->getOrCreateStateSet();
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    ss->setTextureAttributeAndModes(0, _localTex, osg::StateAttribute::ON);

    _view->setSceneData(quad);

    cv->stopThreading();
    cv->addView(_view);
    cv->startThreading();

    _valid = true;
}

OSWindowPlacement::~OSWindowPlacement()
{
    if (!_view) return;

    // Detach readback from canvas camera
    sc::CanvasPtr c = _canvas.lock();
    if (c) {
        if (c->getCamera())
            c->getCamera()->setPostDrawCallback(nullptr);
        c->getProps()->setBoolValue("render-always", false);
    }

    auto* cv = dynamic_cast<osgViewer::CompositeViewer*>(_view->getViewerBase());
    if (cv) {
        cv->stopThreading();
        cv->removeView(_view);
        cv->startThreading();
    }
    _view = nullptr;
}

bool OSWindowPlacement::childChanged(SGPropertyNode* child) { return false; }

sc::Placements
addOSWindowPlacement(SGPropertyNode* placement, sc::CanvasPtr canvas)
{
    auto* p = new OSWindowPlacement(placement, canvas);
    if (!p->isValid()) { delete p; return {}; }
    return {sc::PlacementPtr(p)};
}

} // namespace canvas
