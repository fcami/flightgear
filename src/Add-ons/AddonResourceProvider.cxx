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

#include "Addon.hxx"
#include "AddonManager.hxx"
#include "AddonResourceProvider.hxx"

namespace strutils = simgear::strutils;

using std::string;

namespace flightgear
{

namespace addons
{

ResourceProvider::ResourceProvider()
    // add-on paths can contain any resource type, including NavData
    : simgear::ResourceProvider(simgear::ResourceManager::PRIORITY_DEFAULT, {},
                                simgear::ResourceManager::ProviderCategory::Addon)
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
  if (!addonMgr.get()) {
      return SGPath();
  }

  SGPath addonDir = addonMgr->addonBasePath(addonId);
  SGPath candidate = addonDir / relPath;

  if (!candidate.isFile()) {
    return SGPath();
  }

  return SGPath(candidate).validate(/* write */ false);
}

std::vector<simgear::ResourceManager::FoundFile> ResourceProvider::findAllOfType(simgear::ResourceManager::FileType type) const
{
    const auto& addonMgr = AddonManager::instance();
    if (!addonMgr.get()) {
        return {};
    }


    std::vector<simgear::ResourceManager::FoundFile> files;
    for (const auto& addon : addonMgr->registeredAddons()) {
        const size_t before = files.size();
        const SGPath path = addon->getBasePath();
        findAllOfTypeHelper(path, type, files);
        // add-ons can also ship NavData/<subdir>/*.dat[.gz] files, like a
        // scenery path
        findNavDataHelper(path, type, files);

        // tag every file this add-on contributed, so callers (e.g.
        // NewGUI::init(), for GUIDialog) can tell which add-on a result
        // came from without re-deriving it from the path.
        const std::string origin = "addons/" + addon->getId();
        for (size_t i = before; i < files.size(); ++i) {
            files[i].origin = origin;
        }
    }
    return files;
}

} // of namespace addons

} // of namespace flightgear
