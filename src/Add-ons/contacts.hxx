// SPDX-FileCopyrightText: 2018 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief FlightGear classes holding add-on contact metadata
 */

#pragma once

#include <string>

#include <simgear/structure/SGReferenced.hxx>

#include "addon_fwd.hxx"

namespace nasal
{
  class Hash;                   // forward declaration
};

namespace flightgear
{

namespace addons
{

enum class ContactType {
  author,
  maintainer
};

// Class used to store info about an author or maintainer (possibly also a
// mailing-list, things like that)
class Contact : public SGReferenced
{
public:
  Contact(ContactType type, std::string name, std::string email = "",
          std::string url = "");
  virtual ~Contact() = default;

  ContactType getType() const;
  std::string getTypeString() const;

  std::string getName() const;
  void setName(const std::string& name);

  std::string getEmail() const;
  void setEmail(const std::string& email);

  std::string getUrl() const;
  void setUrl(const std::string& url);

  static void setupGhost(nasal::Hash& addonsModule);

private:
  const ContactType _type;
  std::string _name;
  std::string _email;
  std::string _url;
};

class Author : public Contact
{
public:
  Author(std::string name, std::string email = "", std::string url = "");

  static void setupGhost(nasal::Hash& addonsModule);
};

class Maintainer : public Contact
{
public:
  Maintainer(std::string name, std::string email = "", std::string url = "");

  static void setupGhost(nasal::Hash& addonsModule);
};

// ***************************************************************************
// *                              contact_traits                             *
// ***************************************************************************

template <typename T>
struct contact_traits;

template<>
struct contact_traits<Author>
{
  using contact_type = Author;
  using strong_ref = AuthorRef;

  static std::string xmlNodeName()
  {
    return "author";
  }
};

template<>
struct contact_traits<Maintainer>
{
  using contact_type = Maintainer;
  using strong_ref = MaintainerRef;

  static std::string xmlNodeName()
  {
    return "maintainer";
  }
};

} // of namespace addons

} // namespace flightgear
