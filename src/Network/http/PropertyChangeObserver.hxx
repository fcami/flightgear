// PropertyChangeObserver.hxx -- Watch properties for changes
//
// Written by Torsten Dreyer, started April 2014.
//
// SPDX-FileCopyrightText: 2014 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later
//

#pragma once

#include <simgear/props/props.hxx>
#include <string>
#include <vector>

namespace flightgear {
namespace http {

struct PropertyChangeObserverEntry : public SGReferenced {
    SGPropertyNode_ptr _node;
    std::string _prevValue;
    // Bumped by check() whenever _node's value actually changes. Starts at 1
    // (rather than 0) so that a fresh subscriber -- whose own "last seen"
    // counter always starts at 0, see checkAndConsume() -- is notified once
    // immediately
    unsigned long _version = 1;
};

typedef SGSharedPtr<PropertyChangeObserverEntry> PropertyChangeObserverEntryRef;


class PropertyChangeObserver {
public:
  PropertyChangeObserver();
  virtual ~PropertyChangeObserver();

  const SGPropertyNode_ptr addObservation(const std::string propertyName);

  // Returns true if `node`'s value has changed since `lastSeenVersion` was
  // last updated by this call, updating it to the latest version either
  // way. Callers must keep their own lastSeenVersion (initialized to 0)
  // per node they watch.
  bool checkAndConsume(const SGPropertyNode_ptr& node, unsigned long& lastSeenVersion);

  // Recomputes each observed node's value and bumps its version on
  // change. Called from PropertyChangeWebsocket::poll() itself, so does
  // not generally need calling directly.
  void check();

  void clear() { _entries.clear(); }

private:
  typedef std::vector<PropertyChangeObserverEntryRef> Entries_t;
  Entries_t _entries;
};
}  // namespace http
} // namespace flightgear
