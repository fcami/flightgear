// FGButton.cxx -- a simple button/key wrapper class
//
// Written by Torsten Dreyer, started August 2009
// Based on work from David Megginson, started May 2001.
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2001 David Megginson <david@megginson.com>
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#include <config.h>

#include "FGButton.hxx"


FGButton::FGButton ()
  : is_repeatable(false),
    interval_sec(0),
    delay_sec(0),
    release_delay_sec(0),
    last_dt(0),
    last_state(0)
{
}

FGButton::~FGButton ()
{
  // bindings is a list of SGSharedPtr<SGBindings>
  // no cleanup required
}


void FGButton::init(const SGPropertyNode* node, const std::string& name,
                    const std::string& module)
{
  if (node == 0) {
      SG_BULK_LOG(SG_INPUT, "No bindings for button " << name);
  } else {
    is_repeatable = node->getBoolValue("repeatable", is_repeatable);
    // Get the bindings for the button
    read_bindings( node, bindings, KEYMOD_NONE, module );
  }
}

void FGButton::update( int modifiers, bool pressed, int x, int y)
{
  if (pressed) {
    // The press event may be repeated.
    if (!last_state || is_repeatable) {
        SG_BULK_LOG(SG_INPUT, "FGButton: Button has been pressed");
        for (unsigned int k = 0; k < bindings[modifiers].size(); k++) {
            bindings[modifiers][k]->fire(x, y);
        }
    }
  } else {
    // The release event is never repeated.
    if (last_state) {
        SG_BULK_LOG(SG_INPUT, "FGButton: Button has been released");
        for (unsigned int k = 0; k < bindings[modifiers | KEYMOD_RELEASED].size(); k++)
            bindings[modifiers | KEYMOD_RELEASED][k]->fire(x, y);
    }
  }

  last_state = pressed;
}
