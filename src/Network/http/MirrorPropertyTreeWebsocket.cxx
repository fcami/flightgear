// MirrorPropertyTreeWebsocket.cxx -- A websocket for mirroring a property sub-tree
//
// SPDX-FileCopyrightText: 2016 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MirrorPropertyTreeWebsocket.hxx"
#include "jsonprops.hxx"

#include <algorithm>
#include <unordered_map>
#include <set>

#include <simgear/debug/logstream.hxx>
#include <simgear/props/props.hxx>
#include <simgear/structure/commands.hxx>

#include <simgear/props/props_io.hxx>
#include <Main/globals.hxx>
#include <Main/fg_props.hxx>

#include <nlohmann/json.hpp>

//#define MIRROR_DEBUG 1

namespace flightgear {
namespace http {

using std::string;

    typedef unsigned int PropertyId; // connection local property id

    struct PropertyValue
    {
        PropertyValue(SGPropertyNode* cur = nullptr) :
            type(simgear::props::NONE)
        {
            if (!cur) {
                return;
            }

            type = cur->getType();
            switch (type) {
            case simgear::props::INT:
                intValue = cur->getIntValue();
                break;

            case simgear::props::BOOL:
                intValue = cur->getBoolValue();
                break;

            case simgear::props::FLOAT:
            case simgear::props::DOUBLE:
                doubleValue = cur->getDoubleValue();
                break;

            case simgear::props::STRING:
            case simgear::props::UNSPECIFIED:
                stringValue = cur->getStringValue();
                break;

            case simgear::props::NONE:
                break;

            default:
                SG_LOG(SG_NETWORK, SG_DEV_ALERT, "MirrorPropTree PropertyValue : implement me!" << type);
                break;
            }
        }

        bool equals(SGPropertyNode* node, const PropertyValue& other) const
        {
            if (other.type != type) return false;
            switch (type) {
            case simgear::props::INT:
            case simgear::props::BOOL:
                return intValue == other.intValue;

            case simgear::props::FLOAT:
            case simgear::props::DOUBLE:
                return std::fabs(doubleValue - other.doubleValue) < 1e-4;

            case simgear::props::STRING:
            case simgear::props::UNSPECIFIED:
                return stringValue == other.stringValue;

            case simgear::props::NONE:
                return true;

            default:
                break;
            }

            return false;
        }

        simgear::props::Type type;
        union {
            int intValue;
            double doubleValue;
        };
        std::string stringValue;
    };


    struct RemovedNode
    {
        RemovedNode(SGPropertyNode* node, unsigned int aId) :
            path(node->getPath()),
            id(aId)
        {}

        std::string path;
        unsigned int id = 0;

        bool operator==(const RemovedNode& other) const
        {
            return (path == other.path);
        }
    };

    class MirrorTreeListener : public SGPropertyChangeListener
    {
    public:
        MirrorTreeListener() : SGPropertyChangeListener(true /* recursive */)
        {
            previousValues.resize(2);
        }

        virtual ~MirrorTreeListener()
        {
        }

        void valueChanged(SGPropertyNode* node) override
        {
            auto it = idHash.find(node);
            if (it == idHash.end()) {
                // not new to the server, but new to the client
                newNodes.insert(node);
            } else {
                assert(previousValues.size() > it->second);
                PropertyValue newVal(node);
                if (!previousValues[it->second].equals(node, newVal)) {
                    previousValues[it->second] = newVal;
                    changedNodes.insert(node);
                }
            }
        }

        void childAdded(SGPropertyNode* parent, SGPropertyNode* child) override
        {
            SG_UNUSED(parent);
            recursiveAdd(child);
        }

        void recursiveAdd(SGPropertyNode* node)
        {
            RemovedNode r(node, 0 /* id not actually used */);
#if defined (MIRROR_DEBUG)
            SG_LOG(SG_NETWORK, SG_INFO, "looking for RR:" << r.path);
#endif
            auto rrIt = std::find(recentlyRemoved.begin(), recentlyRemoved.end(), r);
            if (rrIt != recentlyRemoved.end()) {
#if defined (MIRROR_DEBUG)
                SG_LOG(SG_NETWORK, SG_INFO, "recycling node:" << node->getPath());
#endif
                const auto id = rrIt->id;
                // recycle nodes which get thrashed from Nasal (deleted + re-created
                // each time a Nasal timer fires)
                removedNodes.erase(id); // don't remove it!
                idHash.insert(std::make_pair(node, id));

                // we can still do change compression here, but this also
                // deals with type mutation when removing + re-adding with a
                // different type
                PropertyValue newVal(node);
                if (!previousValues[id].equals(node, newVal)) {
                    previousValues[id] = newVal;
                    changedNodes.insert(node);
#if defined (MIRROR_DEBUG)
                    SG_LOG(SG_NETWORK, SG_INFO, "\tand will actually change" << node->getPath());
#endif
                }

                recentlyRemoved.erase(rrIt);
                return;
            }
#if defined (MIRROR_DEBUG)
            SG_LOG(SG_NETWORK, SG_INFO, "new node:" << node->getPath());
#endif
            newNodes.insert(node);
            int child = 0;
            for (; child < node->nChildren(); ++child) {
                recursiveAdd(node->getChild(child));
            }
        }

        void childRemoved(SGPropertyNode* parent, SGPropertyNode* child) override
        {
            changedNodes.erase(child); // have to do this here with the pointer valid
            newNodes.erase(child);

            auto it = idHash.find(child);
            if (it != idHash.end()) {
                removedNodes.insert(it->second);
                idHash.erase(it);
                // record so we can map removed+add of the same property into
                // a simple value change (this happens commonly with the canvas
                // due to lazy Nasal scripting)
                recentlyRemoved.emplace_back(child, it->second);
#if defined (MIRROR_DEBUG)
                SG_LOG(SG_NETWORK, SG_INFO, "adding RR:" << recentlyRemoved.back().path);
#endif
            }
#if defined (MIRROR_DEBUG)
            SG_LOG(SG_NETWORK, SG_INFO, "saw remove of:" << child->getPath());
#endif
        }

        void registerSubtree(SGPropertyNode* node)
        {
#if defined(MIRROR_DEBUG)
            SG_LOG(SG_NETWORK, SG_INFO, "register subtree:" << node->getPath());
#endif
            valueChanged(node);

            // and recurse
            int child = 0;
            for (; child < node->nChildren(); ++child) {
                registerSubtree(node->getChild(child));
            }
        }

        std::set<SGPropertyNode*> newNodes;
        std::set<SGPropertyNode*> changedNodes;
        std::set<PropertyId> removedNodes;

        PropertyId idForProperty(SGPropertyNode* prop)
        {
            auto it = idHash.find(prop);
            if (it == idHash.end()) {
                it = idHash.insert(it, std::make_pair(prop, nextPropertyId++));
                previousValues.push_back(PropertyValue(prop));
            }
            return it->second;
        }

        nlohmann::json makeJSONData()
        {
            using njson = nlohmann::json;
#if defined (MIRROR_DEBUG)
            SGTimeStamp st;
            st.stamp();

            int newSize = newNodes.size();
            int changedSize = changedNodes.size();
            int removedSize = removedNodes.size();
#endif
            njson result;
            if (!newNodes.empty()) {
                njson newNodesJson;

                for (auto prop : newNodes) {
                    changedNodes.erase(prop); // avoid duplicate send

                    njson newPropData = {
                        {"path", prop->getPath(true)},
                        {"type", JSON::getPropertyTypeString(prop->getType())},
                        {"index", prop->getIndex()},
                        {"position", prop->getPosition()},
                        {"id", idForProperty(prop)}};

                    if (prop->getType() != simgear::props::NONE) {
                        newPropData["value"] = JSON::valueToJson(prop);
                    }
                    newNodesJson.push_back(newPropData);
                }

                newNodes.clear();
                result["created"] = newNodesJson;
            }

            if (!removedNodes.empty()) {
                njson removedNodesJson;
                for (auto propId : removedNodes) {
                    removedNodesJson.push_back(propId);
                }
                result["removed"] = removedNodesJson;
                removedNodes.clear();
            }

            if (!changedNodes.empty()) {
                njson changedNodesJson;
                for (auto prop : changedNodes) {
                    changedNodesJson.push_back({idForProperty(prop), JSON::valueToJson(prop)});
                }
                changedNodes.clear();
                result["changed"] = changedNodesJson;
            }
#if defined (MIRROR_DEBUG)
            SG_LOG(SG_NETWORK, SG_INFO, "making JSON data took:" << st.elapsedMSec() << " for " << newSize << "/" << changedSize << "/" << removedSize);
#endif
            recentlyRemoved.clear();

            return result;
        }

        bool haveChangesToSend() const
        {
            return !newNodes.empty() || !changedNodes.empty() || !removedNodes.empty();
        }
    private:
        PropertyId nextPropertyId = 1;
        std::unordered_map<SGPropertyNode*, PropertyId> idHash;
        std::vector<PropertyValue> previousValues;

        /// track recently removed nodes in case they are re-created imemdiately
        /// after with the same type, since we can make this much more efficient
        /// when sending over the wire.
        std::vector<RemovedNode> recentlyRemoved;
    };

#if 0

static void handleSetCommand(const string_list& nodes, cJSON* json, WebsocketWriter &writer)
{
  cJSON * value = cJSON_GetObjectItem(json, "value");
  if ( NULL != value ) {
    if (nodes.size() > 1) {
      SG_LOG(SG_NETWORK, SG_WARN, "httpd: WS set: insufficient values for nodes:" << nodes.size());
      return;
    }

    SGPropertyNode_ptr n = fgGetNode(nodes.front());
    if (!n) {
      SG_LOG(SG_NETWORK, SG_WARN, "httpd: set '" << nodes.front() << "'  not found");
      return;
    }

    setPropertyFromJson(n, value);
    return;
  }

  cJSON * values = cJSON_GetObjectItem(json, "values");
  if ( ( NULL == values ) || ( static_cast<size_t>(cJSON_GetArraySize(values)) != nodes.size()) ) {
    SG_LOG(SG_NETWORK, SG_WARN, "httpd: WS set: mismatched nodes/values sizes:" << nodes.size());
    return;
  }

  string_list::const_iterator it;
  int i=0;
  for (it = nodes.begin(); it != nodes.end(); ++it, ++i) {
    SGPropertyNode_ptr n = fgGetNode(*it);
    if (!n) {
      SG_LOG(SG_NETWORK, SG_WARN, "httpd: get '" << *it << "'  not found");
      return;
    }

    setPropertyFromJson(n, cJSON_GetArrayItem(values, i));
  } // of nodes iteration
}

static void handleExecCommand(cJSON* json)
{
  cJSON* name = cJSON_GetObjectItem(json, "fgcommand");
  if ((NULL == name )|| (NULL == name->valuestring)) {
    SG_LOG(SG_NETWORK, SG_WARN, "httpd: exec: no fgcommand name");
    return;
  }

  SGPropertyNode_ptr arg(new SGPropertyNode);
  JSON::addChildrenToProp( json, arg );

  globals->get_commands()->execute(name->valuestring, arg);
}
#endif

MirrorPropertyTreeWebsocket::MirrorPropertyTreeWebsocket(const std::string& path) :
    _rootPath(path),
    _listener(new MirrorTreeListener),
    _minSendInterval(100)
{
    checkNodeExists();
}

MirrorPropertyTreeWebsocket::~MirrorPropertyTreeWebsocket()
{
}

void MirrorPropertyTreeWebsocket::close()
{
    if (_subtreeRoot) {
        _subtreeRoot->removeChangeListener(_listener.get());
    }
}

void MirrorPropertyTreeWebsocket::checkNodeExists()
{
    _subtreeRoot = globals->get_props()->getNode(_rootPath, false);
    if (_subtreeRoot) {
        _subtreeRoot->addChangeListener(_listener.get());
        _listener->registerSubtree(_subtreeRoot);
        _lastSendTime = SGTimeStamp::now();
    }
}

void MirrorPropertyTreeWebsocket::handleRequest(const HTTPRequest & request, WebsocketWriter &writer)
{
    if (!_subtreeRoot) {
        checkNodeExists();
        if (!_subtreeRoot) {
            return; // still no node exists, we can't process this
        }
    }

  if (request.Content.empty()) return;
#if 0
  /*
   * allowed JSON is
   {
   command : 'addListener',
   nodes : [
   '/bar/baz',
   '/foo/bar'
   ],
   node: '/bax/foo'
   }
   */
  cJSON * json = cJSON_Parse(request.Content.c_str());
  if ( NULL != json) {
    string command;
    cJSON * j = cJSON_GetObjectItem(json, "command");
    if ( NULL != j && NULL != j->valuestring) {
      command = j->valuestring;
    }

    // handle a single node name, or an array of them
    string_list nodeNames;
    j = cJSON_GetObjectItem(json, "node");
    if ( NULL != j && NULL != j->valuestring) {
        nodeNames.push_back(simgear::strutils::strip(string(j->valuestring)));
    }

    cJSON * nodes = cJSON_GetObjectItem(json, "nodes");
    if ( NULL != nodes) {
      for (int i = 0; i < cJSON_GetArraySize(nodes); i++) {
        cJSON * node = cJSON_GetArrayItem(nodes, i);
        if ( NULL == node) continue;
        if ( NULL == node->valuestring) continue;
        nodeNames.push_back(simgear::strutils::strip(string(node->valuestring)));
      }
    }

    if (command == "get") {
      handleGetCommand(nodeNames, writer);
    } else if (command == "set") {
      handleSetCommand(nodeNames, json, writer);
    } else if (command == "exec") {
      handleExecCommand(json);
    } else {
      string_list::const_iterator it;
      for (it = nodeNames.begin(); it != nodeNames.end(); ++it) {
        _watchedNodes.handleCommand(command, *it, _propertyChangeObserver);
      }
    }

    cJSON_Delete(json);
  }
  #endif
}

void MirrorPropertyTreeWebsocket::poll(WebsocketWriter & writer)
{
    if (!_subtreeRoot) {
        checkNodeExists();
        if (!_subtreeRoot) {
            return;
        }
    }

    if (!_listener->haveChangesToSend()) {
        return;
    }

    if (_lastSendTime.elapsedMSec() < _minSendInterval) {
        return;
    }

    // okay, we will send now, update the send stamp
    _lastSendTime.stamp();

    const auto json = _listener->makeJSONData();
    writer.writeText(json.dump());
}

} // namespace http
} // namespace flightgear
