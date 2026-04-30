// SPDX-FileName: FGPUICompatDialog.cxx
// SPDX-FileComment: XML dialog class without using PUI
// SPDX-FileCopyrightText: Copyright (C) 2022 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GUI/dialog.hxx"
#include "config.h"

#include <utility>

#include "FGPUICompatDialog.hxx"

#include <simgear/debug/BufferedLogCallback.hxx>
#include <simgear/misc/strutils.hxx>
#include <simgear/nasal/cppbind/NasalObject.hxx>
#include <simgear/props/props_io.hxx>
#include <simgear/scene/tsync/terrasync.hxx>
#include <simgear/structure/SGBinding.hxx>

#include <Main/fg_os.hxx>
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Scripting/NasalSys.hxx>
#include <Translations/FGTranslate.hxx>

#include "FGColor.hxx"
#include "PUICompatObject.hxx"
#include "new_gui.hxx"

using namespace std::string_literals;
namespace strutils = simgear::strutils;

////////////////////////////////////////////////////////////

class FGPUICompatDialog::DialogPeer : public nasal::Object
{
public:
    DialogPeer(naRef impl) : nasal::Object(impl)
    {
    }

    void setDialog(FGPUICompatDialog* dlg)
    {
        _dialog = dlg;
    }

    SGSharedPtr<FGPUICompatDialog> dialog() const
    {
        return _dialog.lock();
    }

private:
    // the Nasal peer does not hold an owning reference to the
    // main dialog object (dialogs are owned by the NewGUI subsystem)
    SGWeakPtr<FGPUICompatDialog> _dialog;
};

static naRef f_dialogModuleHash(FGPUICompatDialog& dialog, naContext c)
{
    auto nas = globals->get_subsystem<FGNasalSys>();
    if (!nas) {
        return naNil();
    }

    return nas->getModule(dialog.nasalModule());
}

naRef f_dialogRootObject(FGPUICompatDialog& dialog, naContext c)
{
    return nasal::to_nasal(c, dialog._root);
}

//----------------------------------------------------------------------------

naRef f_makeDialogPeer(const nasal::CallContext& ctx)
{
    return ctx.to_nasal(SGSharedPtr<FGPUICompatDialog::DialogPeer>(
        new FGPUICompatDialog::DialogPeer(ctx.requireArg<naRef>(0))));
}

naRef f_dialogCanResize(FGPUICompatDialog& dialog, naContext c)
{
    return nasal::to_nasal(c, dialog.isFlagSet(FGDialog::WindowFlags::Resizable));
}

void FGPUICompatDialog::setupGhost(nasal::Hash& compatModule)
{
    using NasalGUIDialog = nasal::Ghost<SGSharedPtr<FGPUICompatDialog>>;
    NasalGUIDialog::init("gui.xml.CompatDialog")
        .member("name", &FGPUICompatDialog::nameString)
        .member("title", &FGPUICompatDialog::title, &FGPUICompatDialog::setTitle)
        .member("module", &f_dialogModuleHash)
        .member("geometry", &FGPUICompatDialog::geometry)
        .member("x", &FGPUICompatDialog::getX)
        .member("y", &FGPUICompatDialog::getY)
        .member("width", &FGPUICompatDialog::width)
        .member("height", &FGPUICompatDialog::height)
        .member("windowType", &FGPUICompatDialog::windowType)
        .member("uiVersion", &FGPUICompatDialog::uiVersion)
        .member("resizeable", f_dialogCanResize)
        .member("root", f_dialogRootObject)
        .method("close", &FGPUICompatDialog::requestClose);


    using NasalDialogPeer = nasal::Ghost<SGSharedPtr<DialogPeer>>;
    NasalDialogPeer::init("CompatDialogPeer")
        .bases<nasal::ObjectRef>()
        .method("dialog", &DialogPeer::dialog);

    nasal::Hash dialogHash = compatModule.createHash("Dialog");
    dialogHash.set("new", &f_makeDialogPeer);
}

FGPUICompatDialog::FGPUICompatDialog(SGPropertyNode* props,
                                     std::string translationDomain)
    : FGDialog(props, std::move(translationDomain)),
      _props(props),
      _needsRelayout(false)
{
    _module = "__dlg:" + props->getStringValue("name", "[unnamed]");
    _name = props->getStringValue("name", "[unnamed]");

    const std::string t = configTrValue("title");
    _title = t.empty() ? _name : t;
}

FGPUICompatDialog::~FGPUICompatDialog()
{
    // nothing to do, all work was done in close()
}

void FGPUICompatDialog::close()
{
    if (_peer) {
        _peer->callMethod<void>("onClose");
    }

    _props->setIntValue("lastx", getX());
    _props->setIntValue("lasty", getY());
    // FIXME: save width/height as well?

    auto nas = globals->get_subsystem<FGNasalSys>();
    if (nas) {
        if (_nasal_close) {
            const auto s = _nasal_close->getStringValue();
            nas->createModule(_module.c_str(), _module.c_str(), s.c_str(), s.length(), _props);
        }
        nas->deleteModule(_module.c_str());
    }

    _root->recursiveOnDelete();
    _root.clear();

    _peer.clear();
}

bool FGPUICompatDialog::init()
{
    _windowType = _props->getStringValue("type", "dialog");
    _uiVersion = static_cast<uint32_t>(_props->getIntValue("ui-version", 0));

    try {
        auto nas = globals->get_subsystem<FGNasalSys>();

        nasal::Context ctx;
        nasal::Hash guiModule{nas->getModule("gui"), ctx};
        if (guiModule.isNil()) {
            throw sg_exception("Can't initialize PUICompat Nasal");
        }

        using SelfRef = SGSharedPtr<FGPUICompatDialog>;
        using PeerRef = SGSharedPtr<DialogPeer>;

        auto f = guiModule.get<std::function<PeerRef(std::string, SelfRef)>>("_createDialogPeer");
        if (!f) {
            SG_LOG(SG_GUI, SG_DEV_ALERT, "PUICompat module loaded incorrectly");
            return false;
        }

        _peer = f(_windowType, SelfRef{this});
        _peer->setDialog(this);
        _peer->callMethod<void>("init", nas->wrappedPropsNode(_props));

        SGPropertyNode* nasal = _props->getNode("nasal");
        if (nasal && nas) {
            _nasal_close = nasal->getNode("close");
            SGPropertyNode* open = nasal->getNode("open");
            if (open) {
                const auto s = open->getStringValue();
                nas->createModule(_module.c_str(), _module.c_str(), s.c_str(), s.length(), _props);
            }
        }
        display(_props);
        _peer->callMethod<void>("didBuild");
    } catch (std::exception& e) {
        SG_LOG(SG_GUI, SG_ALERT, "Failed to build dialog:" << e.what());

        return false;
    }

    return true;
}

void FGPUICompatDialog::bringToFront()
{
    _peer->callMethod<void>("bringToFront");
}

void FGPUICompatDialog::runCallback(const std::string& name, SGPropertyNode_ptr args)
{
    auto nas = globals->get_subsystem<FGNasalSys>();
    if (!nas)
        return;

    SGPropertyNode* nasalNode = _props->getNode("nasal");
    if (!nasalNode)
        return;

    auto callbackNode = nasalNode->getChild(name);
    if (!callbackNode) {
        SG_LOG(SG_GUI, SG_DEV_ALERT, "FGPUICompatDialog::runCallback: no Nasal callback '" << name << "' defined on dialog " << _name);
        return;
    }

    auto s = callbackNode->getStringValue();
    auto fileName = _module.c_str();
    nas->handleCommand(_module.c_str(), fileName, s.c_str(), args.get());
}

const char* FGPUICompatDialog::getName()
{
    return _name.c_str();
}

void FGPUICompatDialog::updateValues(const std::string& objectName)
{
    _root->recursiveUpdateValues(objectName);
}

void FGPUICompatDialog::applyValues(const std::string& objectName)
{
    _root->recursiveApply(objectName);
}

void FGPUICompatDialog::update()
{
    _root->recursiveUpdate();

    if (_needsRelayout) {
        relayout();
    }
}

void FGPUICompatDialog::display(SGPropertyNode* props)
{
    _root = PUICompatObject::createForType("group", _props);
    _root->setDialog(this);
    _root->init();

    relayout();
}

void FGPUICompatDialog::relayout()
{
    _needsRelayout = false;

    // map from physical to logical units for PUI
    const double ratio = fgGetDouble("/sim/rendering/gui-pixel-ratio", 1.0);
    const int physicalWidth = fgGetInt("/sim/startup/xsize"),
              physicalHeight = fgGetInt("/sim/startup/ysize");
    const int screenw = static_cast<int>(physicalWidth / ratio),
              screenh = static_cast<int>(physicalHeight / ratio);
#if 0
    bool userx = _props->hasValue("x");
    bool usery = _props->hasValue("y");
    bool userw = _props->hasValue("width");
    bool userh = _props->hasValue("height");
#endif
    int px, py, savex, savey;

    const int pw = _props->getIntValue("width", -1);
    const int ph = _props->getIntValue("height", -1);
    px = savex = _props->getIntValue("x", (screenw - pw) / 2);
    py = savey = _props->getIntValue("y", (screenh - ph) / 2);

#if 0
    // Negative x/y coordinates are interpreted as distance from the top/right
    // corner rather than bottom/left.
    if (userx && px < 0)
        px = screenw - pw + px;
    if (usery && py < 0)
        py = screenh - ph + py;
#endif

    _geometry = SGRectd{static_cast<double>(px), static_cast<double>(py),
                        static_cast<double>(pw), static_cast<double>(ph)};
    _peer->callMethod<void>("geometryChanged");
}

double FGPUICompatDialog::getX() const
{
    return _geometry.x();
}

double FGPUICompatDialog::getY() const
{
    return _geometry.y();
}

double FGPUICompatDialog::width() const
{
    return _geometry.width();
}

double FGPUICompatDialog::height() const
{
    return _geometry.height();
}

SGRectd FGPUICompatDialog::geometry() const
{
    return _geometry;
}

std::string FGPUICompatDialog::nameString() const
{
    return _name;
}

std::string FGPUICompatDialog::nasalModule() const
{
    return _module;
}

void FGPUICompatDialog::requestClose()
{
    auto gui = globals->get_subsystem<NewGUI>();
    gui->closeDialog(_name);
}

// Quite similar to XMLObjectBase._configTrValue() in Nasal/gui/XMLDialog.nas
std::string FGPUICompatDialog::configTrValue(const std::string& nodePath)
    const
{
    const auto node = _props->getNode(nodePath);

    if (!node) {
        return {};
    }

    if (!node->getAttribute(SGPropertyNode::TRANSLATE)) {
        SG_LOG(SG_GUI, SG_DEV_WARN, "PUICompatDialog: config property '" <<
               nodePath << "' of dialog '" << _name <<
               "' is not marked for translation (at " << node->getLocation()
               << ")");
        return node->getStringValue();
    }

    const auto resource = "dialog-"s + _name;
    const auto translationId = strutils::strip(node->getStringValue());
    return FGTranslate(translationDomain()).get(resource, translationId);
}

std::string FGPUICompatDialog::title() const
{
    return _title;
}

void FGPUICompatDialog::setTitle(const std::string& s)
{
    _title = s;
    _peer->callMethod<void>("titleChanged");
}

PUICompatObjectRef FGPUICompatDialog::widgetByName(const std::string& name) const
{
    return _root->widgetByName(name);
}
