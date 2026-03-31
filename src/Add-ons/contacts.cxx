// SPDX-FileCopyrightText: 2018 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief FlightGear classes holding add-on contact metadata
 */

#include <string>
#include <utility>

#include <simgear/nasal/cppbind/Ghost.hxx>
#include <simgear/nasal/cppbind/NasalHash.hxx>
#include <simgear/sg_inlines.h>
#include <simgear/structure/exception.hxx>

#include "addon_fwd.hxx"
#include "contacts.hxx"

using std::string;
using simgear::enumValue;

namespace flightgear
{

namespace addons
{

// ***************************************************************************
// *                                 Contact                                 *
// ***************************************************************************

Contact::Contact(ContactType type, string name, string email, string url)
  : _type(type),
    _name(std::move(name)),
    _email(std::move(email)),
    _url(std::move(url))
{ }

ContactType Contact::getType() const
{ return _type; }

string Contact::getTypeString() const
{
  switch (getType()) {
  case ContactType::author:
    return "author";
  case ContactType::maintainer:
    return "maintainer";
  default:
    throw sg_error("unexpected value for member of "
                   "flightgear::addons::ContactType: " +
                   std::to_string(enumValue(getType())));
  }
}

string Contact::getName() const
{ return _name; }

void Contact::setName(const string& name)
{ _name = name; }

string Contact::getEmail() const
{ return _email; }

void Contact::setEmail(const string& email)
{ _email = email; }

string Contact::getUrl() const
{ return _url; }

void Contact::setUrl(const string& url)
{ _url = url; }

// Static method
void Contact::setupGhost(nasal::Hash& addonsModule)
{
  nasal::Ghost<ContactRef>::init("addons.Contact")
    .member("name", &Contact::getName)
    .member("email", &Contact::getEmail)
    .member("url", &Contact::getUrl);
}

// ***************************************************************************
// *                                 Author                                  *
// ***************************************************************************

Author::Author(string name, string email, string url)
  : Contact(ContactType::author, name, email, url)
{ }

// Static method
void Author::setupGhost(nasal::Hash& addonsModule)
{
  nasal::Ghost<AuthorRef>::init("addons.Author")
    .bases<ContactRef>();
}

// ***************************************************************************
// *                               Maintainer                                *
// ***************************************************************************

Maintainer::Maintainer(string name, string email, string url)
  : Contact(ContactType::maintainer, name, email, url)
{ }

// Static method
void Maintainer::setupGhost(nasal::Hash& addonsModule)
{
  nasal::Ghost<MaintainerRef>::init("addons.Maintainer")
    .bases<ContactRef>();
}

} // of namespace addons

} // of namespace flightgear
