// SPDX-FileCopyrightText: 2016 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include <simgear/misc/sg_dir.hxx>

#include "ApplicationProperties.hxx"

using std::string;
using namespace std::string_literals;

double
ApplicationProperties::getDouble (const char *name, const double def) {
  const SGPropertyNode_ptr n (ApplicationProperties::Properties->getNode (name, false));
  if (n == nullptr) {
      return def;
  }
  return n->getDoubleValue ();
}

SGPath
ApplicationProperties::GetRootPath (const char *sub) {
    if (sub != nullptr) {
        const SGPath subpath{std::string(sub)};

        // relative path to current working dir?
        if (subpath.isRelative()) {
            const SGPath path = simgear::Dir::current().path() / sub;
            if (path.exists()) {
                return path;
            }
        } else if (subpath.exists()) {
            // absolute path
            return subpath;
        }
    }

  // default: relative path to FGROOT
  SGPath path (ApplicationProperties::root);
  if (sub != nullptr) {
      path.append(sub);
  }
  return path;
}

string ApplicationProperties::root = ".";
SGPropertyNode_ptr ApplicationProperties::Properties = new SGPropertyNode;
