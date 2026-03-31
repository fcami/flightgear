// SPDX-FileCopyrightText: 2017 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Exception classes for the FlightGear add-on infrastructure
 */

#include <string>

#include <simgear/structure/exception.hxx>

#include "exceptions.hxx"

using std::string;

namespace flightgear
{

namespace addons
{

namespace errors
{

// ***************************************************************************
// *                    Base class for add-on exceptions                     *
// ***************************************************************************

// Prepending a prefix such as "Add-on error: " would be redundant given the
// messages used in, e.g., the Addon class code.
error::error(const string& message, const string& origin)
  : sg_exception(message, origin)
{ }

error::error(const char* message, const char* origin)
  : error(string(message), string(origin))
{ }

} // of namespace errors

} // of namespace addons

} // of namespace flightgear
