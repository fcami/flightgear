// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#include "FGGroupLayer.hxx"
using std::vector;

FGGroupLayer::FGGroupLayer () {
}

FGGroupLayer::~FGGroupLayer () {
  for (unsigned int i = 0; i < m_layers.size (); ++i)
    delete m_layers[i];
}

void
FGGroupLayer::draw () {
  if (test ()) {
    transform ();
    for (unsigned int i = 0; i < m_layers.size (); ++i) {
      m_layers[i]->draw ();
    }
  }
}

void
FGGroupLayer::addLayer (FGInstrumentLayer * const layer) {
  m_layers.push_back (layer);
}
