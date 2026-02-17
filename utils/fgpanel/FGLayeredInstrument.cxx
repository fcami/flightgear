// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GL_utils.hxx"
#include "FGLayeredInstrument.hxx"
#include "FGTexturedLayer.hxx"

FGLayeredInstrument::FGLayeredInstrument (const int x, const int y, const int w, const int h) :
  FGPanelInstrument (x, y, w, h) {
}

FGLayeredInstrument::~FGLayeredInstrument () {
  for (layer_list::iterator it = m_layers.begin();
       it != m_layers.end();
       ++it) {
    delete *it;
    *it = 0;
  }
}

void
FGLayeredInstrument::draw () {
  if (test ()) {
    for (unsigned int i = 0; i < m_layers.size (); ++i) {
      GL_utils::instance ().glPushMatrix ();
      m_layers[i]->draw ();
      GL_utils::instance ().glPopMatrix ();
    }
  }
}

int
FGLayeredInstrument::addLayer (FGInstrumentLayer * const layer) {
  const int n (m_layers.size ());
  if (layer->getWidth () == -1) {
    layer->setWidth (getWidth ());
  }
  if (layer->getHeight () == -1) {
    layer->setHeight (getHeight ());
  }
  m_layers.push_back (layer);
  return n;
}

int
FGLayeredInstrument::addLayer (const FGCroppedTexture_ptr texture, const int w, const int h) {
  return addLayer (new FGTexturedLayer (texture, w, h));
}

void
FGLayeredInstrument::addTransformation (FGPanelTransformation * const transformation) {
  const int layer (m_layers.size () - 1);
  m_layers[layer]->addTransformation (transformation);
}
