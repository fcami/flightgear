// PropertyChangeObserver.cxx -- Watch properties for changes
//
// Written by Torsten Dreyer, started April 2014.
//
// SPDX-FileCopyrightText: 2014 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later
//

#include "PropertyChangeObserver.hxx"

#include <Main/fg_props.hxx>

using std::string;
namespace flightgear {
namespace http {


PropertyChangeObserver::PropertyChangeObserver() = default;

PropertyChangeObserver::~PropertyChangeObserver() = default;

void PropertyChangeObserver::check()
{
    for (Entries_t::iterator it = _entries.begin(); it != _entries.end();) {
        if (!(*it)->_node.isShared()) {
            // node is no longer used but by us - remove the entry
            it = _entries.erase(it);
            continue; // erase() already returned the next element -- don't also ++it
        }

        const std::string current = (*it)->_node->getStringValue();
        if ((*it)->_prevValue != current) {
            (*it)->_prevValue = current;
            ++(*it)->_version;
        }
        ++it;
    }
}

const SGPropertyNode_ptr PropertyChangeObserver::addObservation( const string propertyName)
{
  for (Entries_t::iterator it = _entries.begin(); it != _entries.end(); ++it) {
    if (propertyName == (*it)->_node->getPath(true) ) {
        // A newly-subscribing connection's own "last seen version" always
        // starts at 0 (see checkAndConsume())
        return (*it)->_node;
    }
  }

  try {
    PropertyChangeObserverEntryRef entry = new PropertyChangeObserverEntry();
    entry->_node = fgGetNode( propertyName, true );
    _entries.push_back( entry );
    return entry->_node;
  }
  catch( string & s ) {
    SG_LOG(SG_NETWORK,SG_WARN,"httpd: can't observer '" << propertyName << "'. Invalid name." );
  }

  SGPropertyNode_ptr empty;
  return empty;
}

bool PropertyChangeObserver::checkAndConsume(const SGPropertyNode_ptr& node, unsigned long& lastSeenVersion)
{
  for (Entries_t::iterator it = _entries.begin(); it != _entries.end(); ++it) {
    PropertyChangeObserverEntryRef entry = *it;
    if (entry->_node != node) continue;

    if (entry->_version != lastSeenVersion) {
        lastSeenVersion = entry->_version;
        return true;
    }
    return false;
  }

  return false;
}

}  // namespace http
}  // namespace flightgear
