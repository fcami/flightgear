// SPDX-FileCopyrightText: 2017 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Exception classes for the FlightGear add-on infrastructure
 */

#pragma once

#include <string>

#include <simgear/structure/exception.hxx>

namespace flightgear
{

namespace addons
{

namespace errors
{

class error : public sg_exception
{
public:
  explicit error(const std::string& message,
                 const std::string& origin = std::string());
  explicit error(const char* message, const char* origin = nullptr);
};

class error_loading_config_file : public error
{ using error::error; /* inherit all constructors */ };

class no_metadata_file_found : public error
{ using error::error; };

class error_loading_metadata_file : public error
{ using error::error; };

class error_loading_menubar_items_file : public error
{ using error::error; };

class duplicate_registration_attempt : public error
{ using error::error; };

class fg_version_too_old : public error
{ using error::error; };

class fg_version_too_recent : public error
{ using error::error; };

class invalid_resource_path : public error
{ using error::error; };

class unable_to_create_addon_storage_dir : public error
{ using error::error; };

} // of namespace errors

} // of namespace addons

} // namespace flightgear
