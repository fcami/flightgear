// CanvasWidget.cxx - XML dialog canvas widget
// SPDX-FileCopyrightText: 2025 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PUICompatObject.hxx"
#include "config.h"

#include <simgear/canvas/Canvas.hxx>

#include <Canvas/canvas_mgr.hxx>
#include <GUI/CanvasWidget.hxx>
#include <GUI/FGPUICompatDialog.hxx>
#include <Main/globals.hxx>
#include <Scripting/NasalSys.hxx>

naRef f_makeCanvasPeer(const nasal::CallContext& ctx)
{
    return ctx.to_nasal(PUICompatObjectRef(
        new CanvasWidget(ctx.requireArg<naRef>(0))));
}

CanvasWidget::CanvasWidget(naRef impl) : PUICompatObject(impl, "canvas")
{
}

void CanvasWidget::setupGhost(nasal::Hash& compatModule)
{
    using CanvasGUIObject = nasal::Ghost<CanvasWidgetRef>;
    CanvasGUIObject::init("gui.xml.CanvasWidget")
        .bases<PUICompatObjectRef>()
        .member("canvas", &CanvasWidget::getCanvas);

    nasal::Hash objectHash = compatModule.createHash("Canvas");
    objectHash.set("new", &f_makeCanvasPeer);
}

void CanvasWidget::init()
{
    PUICompatObject::init();

    createChildCanvas(width(), height());

    auto nas = globals->get_subsystem<FGNasalSys>();

    // should we only do this for <canvas> widgets?
    SGPropertyNode* nasal = _config->getNode("nasal");
    if (nasal && nas) {
        auto module = dialog()->nasalModule();
        auto fileName = "__widget:" + name();
        SGPropertyNode* loadScript = nasal->getNode("load");
        if (loadScript) {
            const auto s = loadScript->getStringValue();
            nas->createModule(module.c_str(), fileName.c_str(), s.c_str(), s.length(), _config);
        }
    }
}

CanvasWidget::~CanvasWidget()
{
    if (_canvas) {
        _canvas->destroy();
    }
}

void CanvasWidget::createChildCanvas(int width, int height)
{
    auto canvasMgr = globals->get_subsystem<CanvasMgr>();
    if (!canvasMgr) {
        SG_LOG(SG_GUI, SG_ALERT, "No CanvasManager available");
        return;
    }

    auto n = name();
    if (n.empty()) {
        SG_LOG(SG_GUI, SG_DEV_ALERT, "<canvas> widget without <name>:" << _config->getLocation());
        n = "gui-anonyous";
    }

    _canvas = canvasMgr->createCanvas(n);
    const int view[2] = {width, height};

    SGPropertyNode* cprops = _canvas->getProps();
    cprops->setIntValue("size[0]", view[0] * 2); // use higher resolution
    cprops->setIntValue("size[1]", view[1] * 2); // for antialias
    cprops->setIntValue("view[0]", view[0]);
    cprops->setIntValue("view[1]", view[1]);
    cprops->setBoolValue("render-always", true);
    cprops->setStringValue("name", n);
}

simgear::canvas::CanvasPtr CanvasWidget::getCanvas() const
{
    return _canvas;
}
