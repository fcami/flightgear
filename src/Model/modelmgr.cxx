/*
 * SPDX-FileName: modelmgr.cxx
 * SPDX-FileComment: manage a collection of 3D models
 * SPDX-FileCopyrightText: 2002 David Megginson
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Main/globals.hxx"
#ifdef _MSC_VER
#  pragma warning( disable: 4355 )
#endif

#include <config.h>

#include <simgear/compiler.h>

#include <algorithm>
#include <functional>
#include <vector>
#include <cstring>

#include <simgear/scene/model/placement.hxx>
#include <simgear/scene/model/modellib.hxx>
#include <simgear/structure/exception.hxx>

#include <Main/fg_props.hxx>
#include <Scenery/scenery.hxx>
#include <Scenery/marker.hxx>

#include <osg/ProxyNode>
#include <osgText/String>

#include "modelmgr.hxx"

using namespace simgear;

namespace {

class CheckInstanceModelLoadedVisitor : public osg::NodeVisitor
{
public:
    CheckInstanceModelLoadedVisitor() :
        osg::NodeVisitor(osg::NodeVisitor::NODE_VISITOR, osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
    {}

    ~CheckInstanceModelLoadedVisitor() = default;

    void apply(osg::Node& node) override
    {
        if (!_loaded)
            return;
        traverse(node);
    }

    void apply(osg::ProxyNode& node) override
    {
        if (!_loaded)
            return;

        for (unsigned i = 0; i < node.getNumFileNames(); ++i) {
            if (node.getFileName(i).empty())
                continue;

            // Check if this is already loaded.
            if (i < node.getNumChildren() && node.getChild(i))
                continue;

            _loaded=false;
            return;
        }
        traverse(node);
    }

    bool isLoaded() const
    {
        return _loaded;
    }

private:
    bool _loaded = true;
};

} // of anonymous namespace

FGModelMgr::FGModelMgr() = default;


FGModelMgr::~FGModelMgr() = default;

void
FGModelMgr::init ()
{
    auto model_nodes = _models->getChildren("model");

    for (auto m : model_nodes) {
        add_model(m);
    }
}

void FGModelMgr::reinit()
{
    shutdown();
    init();
}

void FGModelMgr::shutdown()
{
    for (auto ins : _instances) {
        delete ins;
    }

    _instances.clear();
}

void
FGModelMgr::add_model (SGPropertyNode * node)
{
    const std::string model_path{node->getStringValue("path", "Models/Geometry/glider.ac")};
    if (model_path.empty()) {
        SG_LOG(SG_AIRCRAFT, SG_WARN, "add_model called with empty path");
        return;
    }

    const std::string internal_model{node->getStringValue("internal-model", "external")};

    osg::ref_ptr<osg::Node> object;

    Instance* instance = new Instance;
    instance->loaded_node = node->addChild("loaded");
    instance->loaded_node->setBoolValue(false);

    if (internal_model == "marker") {
        std::string label{node->getStringValue("marker/text", "MARKER")};
        float r = node->getFloatValue("marker/color[0]", 1.0f);
        float g = node->getFloatValue("marker/color[1]", 1.0f);
        float b = node->getFloatValue("marker/color[2]", 1.0f);
        osg::Vec4f color(r, g, b, 1.0f);
        float font_size = node->getFloatValue("marker/size", 1.0f);
        float pin_height = node->getFloatValue("marker/height", 1000.0f);
        float tip_height = node->getFloatValue("marker/tip-height", 0.0f);
        object = fgCreateMarkerNode(osgText::String(label, osgText::String::ENCODING_UTF8), font_size, pin_height, tip_height, color);
    } else if (internal_model == "external") {
        try {
            std::string fullPath = simgear::SGModelLib::findDataFile(model_path);
            if (fullPath.empty()) {
                SG_LOG(SG_AIRCRAFT, SG_ALERT, "add_model: unable to find model with name '" << model_path << "'");
                return;
            }
            object = SGModelLib::loadDeferredModel(fullPath, globals->get_props());
        } catch (const sg_throwable& t) {
            SG_LOG(SG_AIRCRAFT, SG_ALERT, "Error loading " << model_path << ":\n  " << t.getFormattedMessage() << t.getOrigin());
            return;
        }
    } else {
        object = new osg::Node;
        SG_LOG(SG_AIRCRAFT, SG_WARN, "Unsupported internal-model type " << internal_model);
    }

  const std::string modelName{node->getStringValue("name", model_path.c_str())};
  SG_LOG(SG_AIRCRAFT, SG_INFO, "Adding model " << modelName);

  instance->model.reset(new SGModelPlacement);
  SGModelPlacement* model = instance->model.get();
  instance->node = node;

  model->init( object );
    double lon = node->getDoubleValue("longitude-deg"),
        lat = node->getDoubleValue("latitude-deg"),
        elevFt = node->getDoubleValue("elevation-ft");

    model->setPosition(SGGeod::fromDegFt(lon, lat, elevFt));
// Set position and orientation either
// indirectly through property refs
// or directly with static values.
  SGPropertyNode * child = node->getChild("longitude-deg-prop");
  if (child != 0)
    instance->lon_deg_node = fgGetNode(child->getStringValue(), true);

  child = node->getChild("latitude-deg-prop");
  if (child != 0)
    instance->lat_deg_node = fgGetNode(child->getStringValue(), true);

  child = node->getChild("elevation-ft-prop");
  if (child != 0)
    instance->elev_ft_node = fgGetNode(child->getStringValue(), true);

  child = node->getChild("roll-deg-prop");
  if (child != 0)
    instance->roll_deg_node = fgGetNode(child->getStringValue(), true);
  else
    model->setRollDeg(node->getDoubleValue("roll-deg"));

  child = node->getChild("pitch-deg-prop");
  if (child != 0)
    instance->pitch_deg_node = fgGetNode(child->getStringValue(), true);
  else
    model->setPitchDeg(node->getDoubleValue("pitch-deg"));

  child = node->getChild("heading-deg-prop");
  if (child != 0)
    instance->heading_deg_node = fgGetNode(child->getStringValue(), true);
  else
    model->setHeadingDeg(node->getDoubleValue("heading-deg"));

  if (node->hasChild("enable-hot")) {
    osg::Node::NodeMask mask = model->getSceneGraph()->getNodeMask();
    if (node->getBoolValue("enable-hot")) {
      mask |= SG_NODEMASK_TERRAIN_BIT;
    } else {
      mask &= ~SG_NODEMASK_TERRAIN_BIT;
    }
    model->getSceneGraph()->setNodeMask(mask);
  }

      			// Add this model to the global scene graph
  globals->get_scenery()->get_scene_graph()->addChild(model->getSceneGraph());


      			// Save this instance for updating
  add_instance(instance);
}

void
FGModelMgr::bind ()
{
    _models = fgGetNode("/models", true);

    _listener.reset(new Listener(this));
    _models->addChangeListener(_listener.get());
}

void
FGModelMgr::unbind ()
{
    // work-around for FLIGHTGEAR-37D : crash when quitting during
    // early startup
    if (_listener) {
        _models->removeChangeListener(_listener.get());
    }

    _listener.reset();
    _models.clear();
}

namespace
{
double testNan(double val)
{
    if (SGMisc<double>::isNaN(val))
        throw sg_range_exception("value is nan");

    return val;
}
} // namespace

void FGModelMgr::update(double dt)
{
    std::for_each(_instances.begin(), _instances.end(), [](FGModelMgr::Instance* instance) {
        auto model = instance->model.get();
        double roll, pitch, heading;
        roll = pitch = heading = 0.0;
        SGGeod pos = model->getPosition();

        try {
            // Optionally set position from properties
            if (instance->lon_deg_node != 0)
                pos.setLongitudeDeg(testNan(instance->lon_deg_node->getDoubleValue()));
            if (instance->lat_deg_node != 0)
                pos.setLatitudeDeg(testNan(instance->lat_deg_node->getDoubleValue()));
            if (instance->elev_ft_node != 0)
                pos.setElevationFt(testNan(instance->elev_ft_node->getDoubleValue()));

            // Optionally set orientation from properties
            if (instance->roll_deg_node != 0)
                roll = testNan(instance->roll_deg_node->getDoubleValue());
            if (instance->pitch_deg_node != 0)
                pitch = testNan(instance->pitch_deg_node->getDoubleValue());
            if (instance->heading_deg_node != 0)
                heading = testNan(instance->heading_deg_node->getDoubleValue());
        } catch (const sg_range_exception&) {
            std::string path = instance->node->getStringValue("path", "unknown");
            SG_LOG(SG_AIRCRAFT, SG_INFO, "Instance of model " << path
                   << " has invalid values");
            return;
        }

        model->setPosition(pos);
        // Optionally set orientation from properties
        if (instance->roll_deg_node != 0)
            model->setRollDeg(roll);
        if (instance->pitch_deg_node != 0)
            model->setPitchDeg(pitch);
        if (instance->heading_deg_node != 0)
            model->setHeadingDeg(heading);

        instance->model->update();
        instance->checkLoaded();
    });
}

void
FGModelMgr::add_instance (Instance * instance)
{
    _instances.push_back(instance);
}

void
FGModelMgr::remove_instance (Instance * instance)
{
    auto it = std::find(_instances.begin(), _instances.end(), instance);
    if (it != _instances.end()) {
        _instances.erase(it);
        delete instance;
    }
}

bool FGModelMgr::removeModelByNodePath(const std::string& nodePath)
{
    auto ins = findInstanceByNodePath(nodePath);
    if (!ins) {
        return false;
    }

    remove_instance(ins);
    return true;
}

FGModelMgr::Instance*
FGModelMgr::findInstanceByNodePath(const std::string& node_path) const
{
    if (node_path.empty())
        return nullptr;

    SGPropertyNode* node = fgGetNode(node_path, false);
    if (!node)
        return nullptr;

    auto it = std::find_if(_instances.begin(), _instances.end(),
                           [node](const Instance* instance)
    { return instance->node == node; });

    if (it == _instances.end()) {
        return nullptr;
    }

    return *it;
}

////////////////////////////////////////////////////////////////////////
// Implementation of FGModelMgr::Instance
////////////////////////////////////////////////////////////////////////

FGModelMgr::Instance::~Instance ()
{
    auto modelSubgraph = model->getSceneGraph();
    auto scenerySubgraph = globals->get_scenery() ? globals->get_scenery()->get_scene_graph() : nullptr;
    if (modelSubgraph && scenerySubgraph) {
        scenerySubgraph->removeChild(modelSubgraph);
    }
}

bool FGModelMgr::Instance::checkLoaded() const
{
    if (!model)
        return false;

    if (loaded_node->getBoolValue()) {
        return true;
    }

    CheckInstanceModelLoadedVisitor cilv;
    model->getSceneGraph()->accept(cilv);
    const bool loadedNow = cilv.isLoaded();

    if (loadedNow) {
        loaded_node->setBoolValue(true);
    }
    return loadedNow;
}

////////////////////////////////////////////////////////////////////////
// Implementation of FGModelMgr::Listener
////////////////////////////////////////////////////////////////////////

void
FGModelMgr::Listener::childAdded(SGPropertyNode * parent, SGPropertyNode * child)
{
  if (parent->getNameString() != "model" || child->getNameString() != "load")
    return;

  _mgr->add_model(parent);
}

void
FGModelMgr::Listener::childRemoved(SGPropertyNode * parent, SGPropertyNode * child)
{
  if (parent->getNameString() != "models" || child->getNameString() != "model")
    return;

  _mgr->removeModelByNodePath(child->getPath());
}


// Register the subsystem.
SGSubsystemMgr::Registrant<FGModelMgr> registrantFGModelMgr(
    SGSubsystemMgr::DISPLAY);

// end of modelmgr.cxx
