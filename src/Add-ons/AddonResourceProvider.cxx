// SPDX-FileCopyrightText: 2018 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief ResourceProvider subclass for add-on files
 */

#include <string>

#include <simgear/misc/ResourceManager.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/misc/strutils.hxx>

#include "AddonManager.hxx"
#include "AddonResourceProvider.hxx"

namespace strutils = simgear::strutils;

using std::string;

namespace flightgear
{

namespace addons
{

ResourceProvider::ResourceProvider()
  : simgear::ResourceProvider(simgear::ResourceManager::PRIORITY_DEFAULT)
{ }

SGPath
ResourceProvider::resolve(const string& resource, SGPath& context) const
{
  if (!strutils::starts_with(resource, "[addon=")) {
    return SGPath();
  }

  string rest = resource.substr(7); // what follows '[addon='
  auto endOfAddonId = rest.find(']');

  if (endOfAddonId == string::npos) {
    return SGPath();
  }

  string addonId = rest.substr(0, endOfAddonId);
  // Extract what follows '[addon=ADDON_ID]'
  string relPath = rest.substr(endOfAddonId + 1);

  if (relPath.empty()) {
    return SGPath();
  }

  const auto& addonMgr = AddonManager::instance();
  SGPath addonDir = addonMgr->addonBasePath(addonId);
  SGPath candidate = addonDir / relPath;

  if (!candidate.isFile()) {
    return SGPath();
  }

  return SGPath(candidate).validate(/* write */ false);
}

std::vector<SGPath> ResourceProvider::findAllOfType(simgear::ResourceManager::FileType type) const
{
    std::vector<SGPath> paths;
    for (const SGPath& path : AddonManager::instance()->addonBasePaths()) {
        findAllOfTypeHelper(path, type, paths);
    }
    return paths;
}

} // of namespace addons

} // of namespace flightgear
