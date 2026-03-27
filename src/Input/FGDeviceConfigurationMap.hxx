// FGDeviceConfigurationMap.hxx -- a map to access xml device configuration
//
// Written by Torsten Dreyer, started August 2009
// Based on work from David Megginson, started May 2001.
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2001 David Megginson <david@megginson.com>
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#pragma once

#include <simgear/props/props.hxx>
#include <simgear/misc/sg_path.hxx>

#include <map>

class FGDeviceConfigurationMap
{
public:
  FGDeviceConfigurationMap();

  FGDeviceConfigurationMap ( const std::string& relative_path,
                            SGPropertyNode* nodePath,
                            const std::string& nodeName);
  virtual ~FGDeviceConfigurationMap();

  SGPropertyNode_ptr configurationForDeviceName(const std::string& name);

  bool hasConfiguration(const std::string& name) const;

  /**
   * @brief return a string encoding a vendor:device ID pair, as hex.
   */
  static std::string nameForVendorDeviceId(uint32_t vendorDeviceId);

  private:
  void scan_dir(const SGPath & path);

  void readCachedData(const SGPath& path);
  void refreshCacheForFile(const SGPath& path);

  std::string computeSuffix(SGPropertyNode_ptr node);

  void insertEntryIfNew(const std::string& name, const SGPath& path);

  typedef std::map<std::string, SGPropertyNode_ptr> NameNodeMap;
// dictionary of over-ridden configurations, where the config data
  // was explicitly loaded and should be picked over a file search
  NameNodeMap overrideDict;

  typedef std::map<std::string, SGPath> NamePathMap;
// mapping from joystick name to XML configuration file path
  NamePathMap namePathMap;
};
