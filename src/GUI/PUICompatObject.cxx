// PUICompatObject.cxx - XML dialog object without using PUI
// SPDX-FileCopyrightText: 2022 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"
#include "simgear/debug/debug_types.h"
#include "simgear/structure/SGBinding.hxx"

#include "PUICompatObject.hxx"

#include <algorithm>
#include <cmath>
#include <string>

#include <simgear/misc/strutils.hxx>
#include <simgear/nasal/cpputils/integers.hxx>
#include <simgear/props/props.hxx>
#include <simgear/props/props_io.hxx> // for copyProperties

#include <GUI/FGPUICompatDialog.hxx>
#include <GUI/new_gui.hxx>
#include <Main/fg_props.hxx>
#include <Scripting/NasalSys.hxx>
#include <Translations/FGTranslate.hxx>
#include <Translations/LanguageInfo.hxx>

using namespace std::string_literals;
namespace strutils = simgear::strutils;

using flightgear::LanguageInfo;

extern naRef propNodeGhostCreate(naContext c, SGPropertyNode* n);

namespace {
struct CompatObjectClassInfo {
    std::string type;
    uint32_t uiVersion = 1;
    bool allChildrenAllowed = false; ///< set if the widget is a generic container
    std::set<std::string> childTypes;

    bool isChildNode(const SGPropertyNode* node, int uiVersion) const;
};

std::vector<CompatObjectClassInfo> static_nasalClassInfo;

std::vector<CompatObjectClassInfo>::const_iterator findClassInfo(const std::string& type)
{
    return std::find_if(static_nasalClassInfo.begin(), static_nasalClassInfo.end(),
                        [&type](const CompatObjectClassInfo& info) {
                            return info.type == type;
                        });
}

bool CompatObjectClassInfo::isChildNode(const SGPropertyNode* node, int uiVersion) const
{
    // common case for leaf widgets that can't have children
    if (!allChildrenAllowed && childTypes.empty()) {
        return false;
    }

    // see if the name is plausible
    bool couldBeChild = false;
    if (allChildrenAllowed) {
        couldBeChild = findClassInfo(node->getNameString()) != static_nasalClassInfo.end();
    } else {
        couldBeChild = childTypes.find(node->getNameString()) != childTypes.end();
    }

    if (!couldBeChild) {
        return false;
    }

    const auto childClassInfoIt = findClassInfo(node->getNameString());
    if ((childClassInfoIt != static_nasalClassInfo.end()) && (childClassInfoIt->uiVersion > uiVersion)) {
        SG_LOG(SG_GUI, SG_DEV_WARN, "PUICompatDialog: child node '" << node->getNameString() << "'is only supported in ui-version " << childClassInfoIt->uiVersion << ", but dialog has ui-version " << uiVersion);
        return false;
    }

    return true;
}

} // namespace

PUICompatObject::PUICompatObject(naRef impl, const std::string& type)
    : nasal::Object(impl), _type(type)
{
}

PUICompatObject::~PUICompatObject()
{
    auto labelNode = _config->getChild("label");
    if (labelNode) {
        labelNode->removeChangeListener(this);
    }

    if (_value) {
        _value->removeChangeListener(this);
    }
}

naRef f_makeCompatObjectPeer(const nasal::CallContext& ctx)
{
    return ctx.to_nasal(PUICompatObjectRef(
        new PUICompatObject(ctx.requireArg<naRef>(0), ctx.requireArg<std::string>(1))));
}

naRef f_translateString(const PUICompatObject& widget, nasal::CallContext ctx)
{
    const auto key = ctx.requireArg<std::string>(0);
    const auto resource = ctx.getArg<std::string>(1);
    const auto domain = ctx.getArg<std::string>(2);
    return ctx.to_nasal(widget.translateString(key, resource, domain));
}

naRef f_translatePluralString(const PUICompatObject& widget, nasal::CallContext ctx)
{
    const auto cardinal = ctx.requireArg<LanguageInfo::intType>(0);
    const auto key = ctx.requireArg<std::string>(1);
    const auto resource = ctx.getArg<std::string>(2);
    const auto domain = ctx.getArg<std::string>(3);
    return ctx.to_nasal(
        widget.translatePluralString(cardinal, key, resource, domain));
}

// First argument of the Nasal call is the leaf name of a node. Second
// argument is undetermined: if it's an integer n, call
// PUICompatObject::translateWithMaybePlural() with n as the “cardinal number”
// (which will actually be used only if the translatable string has plural
// status “true“); else, call PUICompatObject::translateString().
static naRef f_translateWithMaybePlural(const PUICompatObject& widget,
                                        nasal::CallContext ctx)
{
    using intType = LanguageInfo::intType;

    if (ctx.argc != 2) {
        ctx.runtimeError("translateWithMaybePlural() takes exactly two "
                         "arguments (%d given)", ctx.argc);
    }

    const auto nodeName = ctx.requireArg<std::string>(0);
    const auto elementBody = widget.configValue<std::string>(nodeName);
    const auto maybeInteger = nasal::as_integer<intType>(ctx.args[1]);

    return ctx.to_nasal(maybeInteger ?
                        widget.translateWithMaybePlural(*maybeInteger,
                                                        elementBody)
                        : widget.translateString(elementBody)
        );
}

static naRef f_registerObjectClass(const nasal::CallContext& ctx)
{
    const auto type = ctx.requireArg<std::string>(0);
    auto it = findClassInfo(type);
    if (it != static_nasalClassInfo.end()) {
        ctx.runtimeError("Class info for type '%s' is already registered", type.c_str());
    }

    CompatObjectClassInfo info;
    info.type = type;
    info.uiVersion = ctx.requireArg<uint32_t>(1);

    const auto children = ctx.getArg<std::vector<std::string>>(2);
    if (children.empty()) {
        info.allChildrenAllowed = true;
    } else if (children.size() == 1 && (children[0] == "*")) {
        info.allChildrenAllowed = true;
    } else {
        for (const auto& childType : children) {
            info.childTypes.insert(childType);
        }
    }

    static_nasalClassInfo.push_back(std::move(info));
    return naNil();
}

static naRef f_clearObjectClassInfo(const nasal::CallContext& ctx)
{
    static_nasalClassInfo.clear();
    return naNil();
}

void PUICompatObject::setupGhost(nasal::Hash& compatModule)
{
    using NasalGUIObject = nasal::Ghost<PUICompatObjectRef>;
    NasalGUIObject::init("gui.xml.CompatObject")
        .bases<nasal::ObjectRef>()
        .member("config", &PUICompatObject::config)
        .method("configValue", &PUICompatObject::nasalGetConfigValue)
        .member("value", &PUICompatObject::propertyValue)
        .member("property", &PUICompatObject::property)
        .member("geometry", &PUICompatObject::geometry)
        .member("x", &PUICompatObject::getX)
        .member("y", &PUICompatObject::getY)
        .member("width", &PUICompatObject::width)
        .member("height", &PUICompatObject::height)
        .member("children", &PUICompatObject::children)
        .member("dialog", &PUICompatObject::dialog)
        .member("name", &PUICompatObject::name)
        .member("parent", &PUICompatObject::parent)
        .member("live", &PUICompatObject::isLive)
        .member("visible", &PUICompatObject::visible, &PUICompatObject::setVisible)
        .member("enabled", &PUICompatObject::enabled, &PUICompatObject::setEnabled)
        .member("type", &PUICompatObject::type)
        .member("radioGroup", &PUICompatObject::radioGroupIdent)
        .member("hasBindings", &PUICompatObject::hasBindings)
        .method("show", &PUICompatObject::show)
        .method("activateBindings", &PUICompatObject::activateBindings)
        .method("gridLocation", &PUICompatObject::gridLocation)
        .method("trN", f_translatePluralString)
        .method("tr", f_translateString)
        .method("translateWithMaybePlural", f_translateWithMaybePlural);

    nasal::Hash objectHash = compatModule.createHash("Object");
    objectHash.set("new", &f_makeCompatObjectPeer);
    objectHash.set("registerClass", &f_registerObjectClass);
    objectHash.set("clearClassInfo", &f_clearObjectClassInfo);
}

void PUICompatObject::shutdownGhost()
{
    static_nasalClassInfo.clear();
}

PUICompatObjectRef PUICompatObject::createForType(const std::string& type, SGPropertyNode_ptr config)
{
    auto nas = globals->get_subsystem<FGNasalSys>();
    nasal::Context ctx;

    nasal::Hash guiModule{nas->getModule("gui"), ctx};
    if (guiModule.isNil()) {
        throw sg_exception("Can't initialize PUICompat Nasal");
    }

    auto f = guiModule.get<std::function<PUICompatObjectRef(std::string)>>("_createCompatObject");
    PUICompatObjectRef object = f(type);
    // set config
    object->_config = config;
    return object;
}

void PUICompatObject::init()
{
    const auto uiVersion = dialog()->uiVersion();

    _name = _config->getStringValue("name");
    _label = _config->getStringValue("label");
    bool isLive = _config->getBoolValue("live");

    int parentWidth = 800;
    int parentHeight = 600;

    //bool presetSize = _config->hasValue("width") && _config->hasValue("height");
    int width = _config->getIntValue("width", parentWidth);
    int height = _config->getIntValue("height", parentHeight);
    int x = _config->getIntValue("x", (parentWidth - width) / 2);
    int y = _config->getIntValue("y", (parentHeight - height) / 2);

    setGeometry(SGRectd{static_cast<double>(x), static_cast<double>(y),
                        static_cast<double>(width), static_cast<double>(height)});

    if (_config->hasChild("visible")) {
        _visibleCondition = sgReadCondition(globals->get_props(), _config->getChild("visible"));
    }

    if (_config->hasChild("enable")) {
        _enableCondition = sgReadCondition(globals->get_props(), _config->getChild("enable"));
    }

    if (_config->hasChild("label")) {
        _config->getChild("label")->addChangeListener(this);
    }

    if (_config->hasValue("property")) {
        _value = fgGetNode(_config->getStringValue("property"), true);

        if (isLive) {
            _live = LiveValueMode::Listener;
            if (_value->isTied() || _value->isAlias()) {
                const auto isSafe = _value->getAttribute(SGPropertyNode::LISTENER_SAFE);
                if (!isSafe) {
                    SG_LOG(SG_GUI, SG_DEV_WARN, "Requested live updating of unsafe tied property: " << _value->getPath() << "; please fix this propertty to be non-tied or make it listener-safe explicitly.");

                    // we are kind, support polled mode for now
                    _live = LiveValueMode::Polled;
                }
            }

            if (_live == LiveValueMode::Listener) {
                _value->addChangeListener(this);
            }
        }
    }

    // parse version 2 features
    if (uiVersion >= 2) {
        if (_type == "radio") {
            auto g = _config->getStringValue("radio-group");
            if (g.empty()) {
                SG_LOG(SG_GUI, SG_DEV_WARN, "UIv2 radio button does not specify a group ID (at " << _config->getLocation() << ")");
            }
        }
    }

    const auto bindings = _config->getChildren("binding");
    if (!bindings.empty()) {
        for (auto bindingNode : bindings) {
            const auto cmd = bindingNode->getStringValue("command");
            if (cmd == "nasal") {
                // we need to clone the binding node, so we can unique the
                // Nasal module. Otherwise we always modify the global dialog
                // definition, and cloned dialogs use the same Nasal module for
                // <nasal> bindings, which goes wrong. (Especially, the property
                // inspector)

                // memory ownership works because SGBinding has a ref to its
                // argument node and holds onto it.
                SGPropertyNode_ptr copiedBinding = new SGPropertyNode;
                copyProperties(bindingNode, copiedBinding);
                copiedBinding->setStringValue("module", dialog()->nasalModule());

                bindingNode = copiedBinding;
            }

            _bindings.push_back(SGAbstractBinding::createFromProps(bindingNode, globals->get_props()));
        }
    }


    // children
    auto nasalClassInfo = findClassInfo(_type);
    int nChildren = _config->nChildren();
    for (int i = 0; i < nChildren; i++) {
        auto childNode = _config->getChild(i);
        const auto nodeName = childNode->getNameString();

        if (!nasalClassInfo->isChildNode(childNode, uiVersion)) {
            continue;
        }

        SGSharedPtr<PUICompatObject> childObject = createForType(nodeName, childNode);
        childObject->_parent = this;
        _children.push_back(childObject);
    }

    auto nas = globals->get_subsystem<FGNasalSys>();
    callMethod<void>("init", nas->wrappedPropsNode(_config));

    // recursively init children
    for (auto c : _children) {
        c->init();
    }

    callMethod<void>("postinit");
}


std::string PUICompatObject::radioGroupIdent() const
{
    const auto uiVersion = dialog()->uiVersion();
    if (uiVersion < 2) {
        throw std::runtime_error("radioGroupIdent: Not allowed at UI version < 2");
    }

    return _config->getStringValue("radio-group");
}

naRef PUICompatObject::show(naRef viewParent)
{
    nasal::Context ctx;
    return callMethod<naRef>("show", viewParent);
}

void PUICompatObject::update()
{
    if (_enableCondition) {
        const bool e = _enableCondition->test();
        if (e != _enabled) {
            _enabled = e;
            callMethod<void, bool>("enabledChanged", e);
        }
    }

    if (_visibleCondition) {
        const bool e = _visibleCondition->test();
        if (e != _visible) {
            _visible = e;
            callMethod<void, bool>("visibleChanged", e);
        }
    }

    if (_labelChanged) {
        _labelChanged = false;
        callMethod<void, std::string>("labelChanged", _label);
    }

    if (_value) {
        if (_live == LiveValueMode::Polled) {
            // this is a bit heavy, especially for double-valued numerical
            // properties. Lets's see how it goes.
            const auto nv = _value->getStringValue();
            if (nv != _oldPolledValue) {
                _valueChanged = true;
                _oldPolledValue = nv;
            }
        }

        if (_valueChanged) {
            _valueChanged = false;
            callMethod<void>("valueChanged");
        }
    }
}

void PUICompatObject::updateValue()
{
    if (!_value) {
        return;
    }

    if (_live != LiveValueMode::OnApply) {
        return;
    }

    // avoid updates where the value didn't actually change
    const auto nv = _value->getStringValue();
    if (nv != _oldPolledValue) {
        _valueChanged = true;
        _oldPolledValue = nv;
    }

    // we don't call update here(), it will happen next cycle.
}

void PUICompatObject::apply()
{
    callMethod<void>("apply");
    if (_live == LiveValueMode::OnApply) {
        _valueChanged = false;
    }
}

naRef PUICompatObject::property() const
{
    if (!_value)
        return naNil();

    auto nas = globals->get_subsystem<FGNasalSys>();
    return nas->wrappedPropsNode(_value.get());
}

naRef PUICompatObject::propertyValue(naContext ctx) const
{
    return FGNasalSys::getPropertyValue(ctx, _value);
}


naRef PUICompatObject::config() const
{
    auto nas = globals->get_subsystem<FGNasalSys>();
    return nas->wrappedPropsNode(_config.get());
}

naRef PUICompatObject::nasalGetConfigValue(const nasal::CallContext ctx) const
{
    auto name = ctx.requireArg<std::string>(0);
    naRef defaultVal = ctx.getArg(1, naNil());
    SGPropertyNode_ptr n = _config->getChild(name);
    if (!n || !n->hasValue())
        return defaultVal;

    return FGNasalSys::getPropertyValue(ctx.c_ctx(), n.get());
}

void PUICompatObject::valueChanged(SGPropertyNode* node)
{
    if (node->getNameString() == "label") {
        _labelChanged = true;
        _label = node->getStringValue();
        return;
    }

    if (_live == LiveValueMode::OnApply)
        return;

    // don't fire Nasal callback now, might cause recursion
    _valueChanged = true;
}

void PUICompatObject::activateBindings()
{
    if (!_enabled) {
        SG_LOG(SG_GUI, SG_DEV_ALERT, "Skipping binding activation for disabled widget:" << name());
        return;
    }

    auto guiSub = globals->get_subsystem<NewGUI>();
    assert(guiSub);

    guiSub->setActiveDialog(dialog());
    fireBindingList(_bindings);
    guiSub->setActiveDialog(nullptr);
}

bool PUICompatObject::hasBindings() const
{
    return !_bindings.empty();
}

void PUICompatObject::setGeometry(const SGRectd& g)
{
    updateGeometry(g);
}

void PUICompatObject::updateGeometry(const SGRectd& newGeom)
{
    if (newGeom == _geometry) {
        return;
    }

    _geometry = newGeom;
    callMethod<void>("geometryChanged");

    // cascade to children?
}

double PUICompatObject::getX() const
{
    return _geometry.pos().x();
}

double PUICompatObject::getY() const
{
    return _geometry.pos().y();
}

double PUICompatObject::width() const
{
    return _geometry.width();
}

double PUICompatObject::height() const
{
    return _geometry.height();
}

SGRectd PUICompatObject::geometry() const
{
    return _geometry;
}

PUICompatObjectRef PUICompatObject::parent() const
{
    return _parent.lock();
}

PUICompatObjectVec PUICompatObject::children() const
{
    return _children;
}

bool PUICompatObject::visible() const
{
    if (_visibleCondition) {
        return _visibleCondition->test();
    }

    return _visible;
}

bool PUICompatObject::enabled() const
{
    if (_enableCondition) {
        return _enableCondition->test();
    }

    return _enabled;
}

const std::string& PUICompatObject::type() const
{
    return _type;
}

void PUICompatObject::setVisible(bool v)
{
    if (_visibleCondition) {
        SG_LOG(SG_GUI, SG_DEV_ALERT, "Trying to set visibility on widget with visible condition already defined");
        return;
    }

    if (_visible == v)
        return;

    _visible = v;
    callMethod<void, bool>("visibleChanged", _visible);
}

void PUICompatObject::setEnabled(bool e)
{
    if (_enableCondition) {
        SG_LOG(SG_GUI, SG_DEV_ALERT, "Trying to set enabled on widget with enable condition already defined");
        return;
    }

    if (_enabled == e)
        return;

    _enabled = e;
    callMethod<void, bool>("enabledChanged", _enabled);
}

PUICompatObjectRef PUICompatObject::widgetByName(const std::string& name) const
{
    if (name == _name) {
        return PUICompatObjectRef(const_cast<PUICompatObject*>(this));
    }

    for (auto child : _children) {
        auto r = child->widgetByName(name);
        if (r) {
            return r;
        }
    }

    return {};
}

void PUICompatObject::recursiveUpdate(const std::string& objectName)
{
    if (objectName.empty() || (objectName == _name)) {
        update();
    }

    for (auto child : _children) {
        child->recursiveUpdate(objectName);
    }
}

void PUICompatObject::recursiveUpdateValues(const std::string& objectName)
{
    if (objectName.empty() || (objectName == _name)) {
        updateValue();
    }

    for (auto child : _children) {
        child->recursiveUpdateValues(objectName);
    }
}


void PUICompatObject::recursiveApply(const std::string& objectName)
{
    if (objectName.empty() || (objectName == _name)) {
        apply();
    }

    for (auto child : _children) {
        child->recursiveApply(objectName);
    }
}

void PUICompatObject::recursiveOnDelete()
{
    // bottom up call of del()
    for (auto child : _children) {
        child->recursiveOnDelete();
    }

    callMethod<void>("del");
}

void PUICompatObject::setDialog(PUICompatDialogRef dialog)
{
    _dialog = dialog;
}

PUICompatDialogRef PUICompatObject::dialog() const
{
    PUICompatObjectRef pr = _parent.lock();
    if (pr) {
        return pr->dialog();
    }

    return _dialog.lock();
}

nasal::Hash PUICompatObject::gridLocation(const nasal::CallContext& ctx) const
{
    nasal::Hash result{ctx.c_ctx()};
    result.set("column", _config->getIntValue("col"));
    result.set("row", _config->getIntValue("row"));
    result.set("columnSpan", _config->getIntValue("colspan", 1));
    result.set("rowSpan", _config->getIntValue("rowspan", 1));
    return result;
}

std::string PUICompatObject::translatePluralString(
    LanguageInfo::intType cardinal, const std::string& key,
    const std::string& resource, const std::string& domain) const
{
    auto strippedKey = strutils::strip(key);
    auto res = resource.empty() ? "dialog-"s + dialog()->getName() : resource;
    auto dom = domain.empty() ? dialog()->translationDomain() : domain;
    return FGTranslate(dom).getPluralWithDefault(cardinal, res, strippedKey,
                                                 strippedKey);
}

std::string PUICompatObject::translateString(const std::string& key, const std::string& resource, const std::string& domain) const
{
    auto strippedKey = strutils::strip(key);
    auto res = resource.empty() ? "dialog-"s + dialog()->getName() : resource;
    auto dom = domain.empty() ? dialog()->translationDomain() : domain;
    return FGTranslate(dom).getWithDefault(res, strippedKey, strippedKey);
}

std::string PUICompatObject::translateWithMaybePlural(
    LanguageInfo::intType cardinalNumber, const std::string& key,
    const std::string& resource, const std::string& domain) const
{
    const auto strippedKey = strutils::strip(key);
    const auto res = resource.empty() ? "dialog-"s + dialog()->getName() :
        resource;
    const auto dom = domain.empty() ? dialog()->translationDomain() : domain;

    const auto translUnit = FGTranslate(dom).translationUnit(res, strippedKey);

    if (!translUnit) {
        SG_LOG(SG_GUI, SG_DEV_ALERT, "In '" << dialog()->getName() << "' "
               "dialog: attempt to fetch translation for " << dom << "/" <<
               res << "/" << strippedKey << " which cannot be found (it "
               "seems it is not even in the default translation; it could "
               "be an extractable string that hasn't been extracted yet)");
        return strippedKey;
    }

    return translUnit->getPluralStatus() ?
        translUnit->getTranslation(cardinalNumber) :
        translUnit->getTranslation();
}
