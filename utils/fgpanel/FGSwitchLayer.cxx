// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#include "FGSwitchLayer.hxx"

FGSwitchLayer::FGSwitchLayer () :
  FGGroupLayer () {
}

void
FGSwitchLayer::draw () {
  if (test ()) {
    transform ();
    for (unsigned int i = 0; i < m_layers.size (); ++i) {
      if (m_layers[i]->test ()) {
        m_layers[i]->draw ();
        return;
      }
    }
  }
}
