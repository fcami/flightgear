// PropertyChangeWebsocket.cxx -- A websocket for propertychangelisteners
//
// Written by Torsten Dreyer, started April 2014.
//
// SPDX-FileCopyrightText: 2014 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later
//

#include "PropertyChangeWebsocket.hxx"
#include "PropertyChangeObserver.hxx"
#include "jsonprops.hxx"
#include "simgear/misc/strutils.hxx"

#include <simgear/debug/logstream.hxx>
#include <simgear/props/props.hxx>
#include <simgear/structure/commands.hxx>

#include <simgear/props/props_io.hxx>
#include <Main/globals.hxx>
#include <Main/fg_props.hxx>

#include <nlohmann/json.hpp>

namespace flightgear {
namespace http {

using nlohmann::json;
using std::string;

static unsigned nextid = 0;

static void handleSetCommand(const string_list& nodes, const json& json, WebsocketWriter& writer)
{
    // single value case
    if (json.contains("value")) {
        if (nodes.size() > 1) {
            SG_LOG(SG_NETWORK, SG_WARN, "httpd: WS set: insufficient values for nodes:" << nodes.size());
            return;
        }

        SGPropertyNode_ptr n = fgGetNode(nodes.front());
        if (!n) {
            SG_LOG(SG_NETWORK, SG_WARN, "httpd: set '" << nodes.front() << "'  not found");
            return;
        }

        JSON::setValueFromJSON(json.at("value"), n);
        return;
    }

    // multi-value case
    if (!json.contains("values")) {
        SG_LOG(SG_NETWORK, SG_WARN, "httpd: WS set: neither value or values present");
        return;
    }

    const auto& values = json.at("values");
    if (values.size() != nodes.size()) {
        SG_LOG(SG_NETWORK, SG_WARN, "httpd: WS set: mismatched nodes/values sizes:" << nodes.size());
        return;
    }

    size_t index = 0;
    for (auto nodePath : nodes) {
        SGPropertyNode_ptr n = fgGetNode(nodePath);
        if (!n) {
            SG_LOG(SG_NETWORK, SG_WARN, "httpd: get '" << nodePath << "'  not found");
        } else {
            JSON::setValueFromJSON(values.at(index++), n);
        }
    } // of nodes/values iteration
}

static void handleExecCommand(const json& json)
{
    const auto cmd = json.value<std::string>("fgcommand", {});
    if (cmd.empty()) {
        SG_LOG(SG_NETWORK, SG_WARN, "httpd: exec: no fgcommand name");
        return;
    }

  SGPropertyNode_ptr arg(new SGPropertyNode);
  JSON::addChildrenToProp( json, arg );

  globals->get_commands()->execute(cmd, arg, nullptr);
}

PropertyChangeWebsocket::PropertyChangeWebsocket(PropertyChangeObserver* propertyChangeObserver)
    : id(++nextid),
      _propertyChangeObserver(propertyChangeObserver),
      _minTriggerInterval(fgGetDouble("/sim/http/property-websocket/update-interval-secs", 0.05)) // default 20Hz
{
}

PropertyChangeWebsocket::~PropertyChangeWebsocket() = default;

void PropertyChangeWebsocket::close()
{
  SG_LOG(SG_NETWORK, SG_INFO, "closing PropertyChangeWebsocket #" << id);
  _watchedNodes.clear();
}

void PropertyChangeWebsocket::handleGetCommand(const string_list& nodes, WebsocketWriter &writer)
{
    const double t = fgGetDouble("/sim/time/elapsed-sec");
    for (const auto& nodePath : nodes) {
        SGPropertyNode_ptr n = fgGetNode(nodePath);
        if (!n) {
            SG_LOG(SG_NETWORK, SG_WARN, "httpd: get '" << nodePath << "'  not found");
            return;
        }

        writer.writeText(JSON::toJsonString(false, n, 0, t));
    } // of nodes iteration
}

void PropertyChangeWebsocket::handleRequest(const HTTPRequest & request, WebsocketWriter &writer)
{
  if (request.Content.empty()) return;

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
  json json = json::parse(request.Content, nullptr, false);
  if (json.is_discarded()) {
      SG_LOG(SG_NETWORK, SG_WARN, "httpd: unable to parse request JSON:\n\t" << request.Content);
      return;
  }

  const auto command = json.value<std::string>("command", {});

  // handle a single node name, or an array of them
  string_list nodeNames;
  if (json.contains("node")) {
      nodeNames.push_back(json.at("node").template get<std::string>());
  } else if (json.contains("nodes")) {
      nodeNames = json.at("nodes").template get<string_list>();
  }

  // strip strings in place
  for (auto& s : nodeNames) {
      s = simgear::strutils::strip(s);
  }

  if (command == "get") {
      handleGetCommand(nodeNames, writer);
  } else if (command == "set") {
      handleSetCommand(nodeNames, json, writer);
  } else if (command == "exec") {
      handleExecCommand(json);
  } else {
      for (auto n : nodeNames) {
          _watchedNodes.handleCommand(command, n, _propertyChangeObserver);
      }
  }
}

void PropertyChangeWebsocket::poll(WebsocketWriter & writer)
{
  double now = fgGetDouble("/sim/time/elapsed-sec");

  if( _minTriggerInterval > .0 ) {
    if( now - _lastTrigger <= _minTriggerInterval )
      return;

    _lastTrigger = now;
  }

  // Detect changes since the last poll() call. Each cpnnection owns its
  // PropertyChangeObserver exclusively
  _propertyChangeObserver->check();

  for (auto& watched : _watchedNodes) {
      if (_propertyChangeObserver->checkAndConsume(watched.node, watched.lastSeenVersion)) {
          string out = JSON::toJsonString(false, watched.node, 0, now);
          SG_LOG(SG_NETWORK, SG_BULK, "PropertyChangeWebsocket::poll() new Value for " << watched.node->getPath(true) << " '" << watched.node->getStringValue() << "' #" << id << ": " << out);
          writer.writeText(out);
      }
  }
}

void PropertyChangeWebsocket::WatchedNodesList::handleCommand(const string & command, const string & node,
    PropertyChangeObserver * propertyChangeObserver)
{
  if (command == "addListener") {
    for (iterator it = begin(); it != end(); ++it) {
        if (node == it->node->getPath(true)) {
            SG_LOG(SG_NETWORK, SG_WARN, "httpd: " << command << " '" << node << "' ignored (duplicate)");
            return; // duplicate
        }
    }
    SGPropertyNode_ptr n = propertyChangeObserver->addObservation(node);
    if (n.valid()) push_back(WatchedNode{n});
    SG_LOG(SG_NETWORK, SG_INFO, "httpd: " << command << " '" << node << "' success");

  } else if (command == "removeListener") {
    for (iterator it = begin(); it != end(); ++it) {
        if (node == it->node->getPath(true)) {
            this->erase(it);
            SG_LOG(SG_NETWORK, SG_INFO, "httpd: " << command << " '" << node << "' success");
            return;
        }
    }
    SG_LOG(SG_NETWORK, SG_WARN, "httpd: " << command << " '" << node << "' ignored (not found)");
  }
}

} // namespace http
} // namespace flightgear
