// SPDX-FileCopyrightText: 2018 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief ResourceProvider subclass for add-on files
 */

#pragma once

#include <string>

#include <simgear/misc/ResourceManager.hxx>
#include <simgear/misc/sg_path.hxx>

namespace flightgear
{

namespace addons
{

class ResourceProvider : public simgear::ResourceProvider
{
public:
  ResourceProvider();

  virtual SGPath resolve(const std::string& resource, SGPath& context) const
    override;
  std::vector<SGPath> findAllOfType(simgear::ResourceManager::FileType type) const override;
};

} // of namespace addons

} // namespace flightgear
