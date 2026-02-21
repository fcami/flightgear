// FGCommonInput.cxx -- common functions for all Input subsystems
//
// Written by Torsten Dreyer, started August 2009
// Based on work from David Megginson, started May 2001.
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2001 David Megginson <david@megginson.com>
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#include <config.h>

#include "FGCommonInput.hxx"
#include "simgear/structure/SGBinding.hxx"
#include <Main/fg_os.hxx>
#include <Main/globals.hxx>

using simgear::PropertyList;
using std::string;

void FGCommonInput::read_bindings (const SGPropertyNode * node, binding_list_t * binding_list, int modifiers, const string & module )
{
  PropertyList bindings = node->getChildren("binding");
  static string nasal = "nasal";
  for (unsigned int i = 0; i < bindings.size(); i++) {
    std::string cmd = bindings[i]->getStringValue("command");
    if (nasal.compare(cmd) == 0 && !module.empty())
      bindings[i]->setStringValue("module", module.c_str());

    auto b = SGAbstractBinding::createFromProps(bindings[i], globals->get_props());
    binding_list[modifiers].push_back(b);
  }

                                // Read nested bindings for modifiers
  if (node->getChild("mod-up") != 0)
    read_bindings(node->getChild("mod-up"), binding_list,
                   modifiers|KEYMOD_RELEASED, module);

  if (node->getChild("mod-shift") != 0)
    read_bindings(node->getChild("mod-shift"), binding_list,
                   modifiers|KEYMOD_SHIFT, module);

  if (node->getChild("mod-ctrl") != 0)
    read_bindings(node->getChild("mod-ctrl"), binding_list,
                   modifiers|KEYMOD_CTRL, module);

  if (node->getChild("mod-alt") != 0)
    read_bindings(node->getChild("mod-alt"), binding_list,
                   modifiers|KEYMOD_ALT, module);

  if (node->getChild("mod-meta") != 0)
    read_bindings(node->getChild("mod-meta"), binding_list,
                   modifiers|KEYMOD_META, module);

  if (node->getChild("mod-super") != 0)
    read_bindings(node->getChild("mod-super"), binding_list,
                   modifiers|KEYMOD_SUPER, module);

  if (node->getChild("mod-hyper") != 0)
    read_bindings(node->getChild("mod-hyper"), binding_list,
                   modifiers|KEYMOD_HYPER, module);
}
