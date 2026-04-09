// FGDeviceConfigurationMap.cxx -- a map to access xml device configuration
//
// Written by Torsten Dreyer, started August 2009
// Based on work from David Megginson, started May 2001.
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2001 David Megginson <david@megginson.com>
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#include <config.h>

#include "FGDeviceConfigurationMap.hxx"

#include <simgear/debug/ErrorReportingCallback.hxx>
#include <simgear/misc/sg_dir.hxx>
#include <simgear/props/props_io.hxx>
#include <simgear/structure/exception.hxx>

#include <Main/globals.hxx>
#include <Main/sentryIntegration.hxx>
#include <Navaids/NavDataCache.hxx>
#include <string>

using simgear::PropertyList;
using std::string;
using namespace std::string_literals;

std::string FGDeviceConfigurationMap::nameForVendorDeviceId(uint32_t vendorDeviceId)
{
    const uint16_t v = (vendorDeviceId >> 16) & 0xFFFF;
    const uint16_t d = vendorDeviceId & 0xFFFF;

    std::ostringstream os;
    os << std::hex << "vendor:0x" << v << ":device:0x" << d;
    return os.str();
}

FGDeviceConfigurationMap::FGDeviceConfigurationMap() = default;

FGDeviceConfigurationMap::FGDeviceConfigurationMap( const string& relative_path,
                                                   SGPropertyNode* nodePath,
                                                   const std::string& nodeName)
{
  // scan for over-ride configurations, loaded via joysticks.xml, etc
  for (auto preloaded : nodePath->getChildren(nodeName)) {
    std::string suffix = computeSuffix(preloaded);
    for (auto nameProp : preloaded->getChildren("name")) {
      overrideDict[nameProp->getStringValue() + suffix] = preloaded;
    } // of names iteration
  } // of defined overrides iteration

  scan_dir( SGPath(globals->get_fg_home(), relative_path));
  scan_dir( SGPath(globals->get_fg_root(), relative_path));
}

std::string FGDeviceConfigurationMap::computeSuffix(SGPropertyNode_ptr node)
{
    if (node->hasChild("serial-number")) {
        return "::"s + node->getStringValue("serial-number");
    }

    // allow specifying a device number / index in the override
    if (node->hasChild("device-number")) {
        return "_"s + std::to_string(node->getIntValue("device-number"));
    }

    return{};
}

FGDeviceConfigurationMap::~FGDeviceConfigurationMap() = default;

SGPropertyNode_ptr
FGDeviceConfigurationMap::configurationForDeviceName(const std::string& name)
{
  auto j = overrideDict.find(name);
  if (j != overrideDict.end()) {
    return j->second;
  }

// no override, check out list of config files
  auto it = namePathMap.find(name);
  if (it == namePathMap.end()) {
    return {};
  }

  SGPropertyNode_ptr result(new SGPropertyNode);
  try {
      readProperties(it->second, result);
      result->setStringValue("source", it->second.utf8Str());
  } catch (sg_exception& e) {
      simgear::reportFailure(simgear::LoadFailure::BadData, simgear::ErrorCode::InputDeviceConfig,
                             "Failed to parse device configuration:" + e.getFormattedMessage(),
                             it->second);
      return {};
  }

  return result;
}

bool FGDeviceConfigurationMap::hasConfiguration(const std::string& name) const
{
  auto j = overrideDict.find(name);
  if (j != overrideDict.end()) {
    return true;
  }

  return namePathMap.find(name) != namePathMap.end();
}

void FGDeviceConfigurationMap::scan_dir(const SGPath & path)
{
  SG_LOG(SG_INPUT, SG_DEBUG, "Scanning " << path << " for input devices");
  if (!path.exists())
    return;

  flightgear::NavDataCache* cache = flightgear::NavDataCache::instance();
  flightgear::NavDataCache::Transaction txn(cache);

  simgear::Dir dir(path);
  simgear::PathList children = dir.children(simgear::Dir::TYPE_FILE |
                                            simgear::Dir::TYPE_DIR | simgear::Dir::NO_DOT_OR_DOTDOT);

  for (SGPath path : children) {
    if (path.isDir()) {
      scan_dir(path);
    } else if (path.extension() == "xml") {
      if (cache->isCachedFileModified(path)) {
        refreshCacheForFile(path);
      } else {
        readCachedData(path);
      } // of cached file stamp is valid
    } // of child is a file with '.xml' extension
  } // of directory children iteration

  txn.commit();
}

void FGDeviceConfigurationMap::readCachedData(const SGPath& path)
{
  auto cache = flightgear::NavDataCache::instance();
  for (string s : cache->readStringListProperty(path.utf8Str())) {
      insertEntryIfNew(s, path);
  } // of cached names iteration
}

void FGDeviceConfigurationMap::insertEntryIfNew(const std::string& name, const SGPath& path)
{
    if (namePathMap.find(name) == namePathMap.end()) {
        namePathMap.insert(std::make_pair(name, path));
    }
}

void FGDeviceConfigurationMap::refreshCacheForFile(const SGPath& path)
{
    simgear::ErrorReportContext ectx("input-device", path.utf8Str());

    SG_LOG(SG_INPUT, SG_DEBUG, "Reading device file " << path);
    SGPropertyNode_ptr n(new SGPropertyNode);
    try {
        readProperties(path, n);
    } catch (sg_exception& e) {
        simgear::reportFailure(simgear::LoadFailure::BadData, simgear::ErrorCode::InputDeviceConfig,
                               "Failed to load input device configuration:" + e.getFormattedMessage(),
                               path);
        return;
    }

  std::string suffix = computeSuffix(n);
  string_list names;
  for (auto nameProp : n->getChildren("name")) {
    const string name = nameProp->getStringValue() + suffix;
    names.push_back(name);
    insertEntryIfNew(name, path);
  }

  if (n->hasChild("device-id") && n->hasChild("vendor-id")) {
      try {
          uint32_t vendorId = simgear::strutils::to_int(n->getStringValue("vendor-id"), 0 /* auto-detect base */);
          uint32_t deviceId = simgear::strutils::to_int(n->getStringValue("device-id"), 0 /* auto-detect base */);
          const auto vendorDeviceId = (vendorId << 16) | deviceId;
          const auto name = nameForVendorDeviceId(vendorDeviceId);
          if (!name.empty()) {
              names.push_back(name);
              insertEntryIfNew(name, path);
          }
      } catch (const std::exception& e) {
          simgear::reportFailure(simgear::LoadFailure::BadData, simgear::ErrorCode::InputDeviceConfig,
                                 "Invalid vendor/device ID:"s + e.what(),
                                 path);
      }
  }

  auto cache = flightgear::NavDataCache::instance();
  if (!cache->isReadOnly()) {
      cache->stampCacheFile(path);
      cache->writeStringListProperty(path.utf8Str(), names);
  }
}
