// SPDX-FileCopyrightText: 2017 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Forward declarations for the FlightGear add-on infrastructure
 */

#pragma once

#include <simgear/structure/SGSharedPtr.hxx>

namespace flightgear
{

namespace addons
{

class Addon;
class AddonManager;
class AddonVersion;
class AddonVersionSuffix;
class ResourceProvider;

enum class UrlType;
class QualifiedUrl;

enum class ContactType;
class Contact;
class Author;
class Maintainer;

using AddonRef = SGSharedPtr<Addon>;
using AddonVersionRef = SGSharedPtr<AddonVersion>;
using ContactRef = SGSharedPtr<Contact>;
using AuthorRef = SGSharedPtr<Author>;
using MaintainerRef = SGSharedPtr<Maintainer>;

namespace errors
{

class error;
class error_loading_config_file;
class no_metadata_file_found;
class error_loading_metadata_file;
class error_loading_menubar_items_file;
class duplicate_registration_attempt;
class fg_version_too_old;
class fg_version_too_recent;
class invalid_resource_path;
class unable_to_create_addon_storage_dir;

} // of namespace errors

} // of namespace addons

} // namespace flightgear
