// PropertyChangeWebsocket.hxx -- A websocket for propertychangelisteners
//
// Written by Torsten Dreyer, started April 2014.
//
// SPDX-FileCopyrightText: 2014 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later
//

#pragma once

#include "Websocket.hxx"
#include <simgear/props/props.hxx>

#include <vector>

namespace flightgear {
namespace http {

class PropertyChangeObserver;

class PropertyChangeWebsocket: public Websocket {
public:
  PropertyChangeWebsocket(PropertyChangeObserver * propertyChangeObserver);
  virtual ~PropertyChangeWebsocket();
  virtual void close();
  virtual void handleRequest(const HTTPRequest & request, WebsocketWriter & writer);
  virtual void poll(WebsocketWriter & writer);

private:
  unsigned id;
  PropertyChangeObserver * _propertyChangeObserver;

  void handleGetCommand(const string_list& nodes, WebsocketWriter &writer);

  // One entry per node this connection watches. lastSeenVersion is this
  // connection's own read cursor into PropertyChangeObserver's per-node
  // version counter (see PropertyChangeObserver.hxx) -- each connection
  // tracks its own, rather than sharing one consume-once flag, so that
  // multiple simultaneous /PropertyListener connections watching the same
  // node each reliably see every change exactly once.
  struct WatchedNode {
      SGPropertyNode_ptr node;
      unsigned long lastSeenVersion = 0;
  };

  class WatchedNodesList : public std::vector<WatchedNode>
  {
  public:
    void handleCommand(const std::string & command, const std::string & node, PropertyChangeObserver * propertyChangeObserver);
  };

  WatchedNodesList _watchedNodes;
  double _minTriggerInterval;
  double _lastTrigger = -1000.0;
};

}
} // namespace flightgear
