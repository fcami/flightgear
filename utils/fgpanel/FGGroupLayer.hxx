// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FGGROUPLAYER_HXX
#define FGGROUPLAYER_HXX

#include "FGInstrumentLayer.hxx"
using std::vector;

/**
 * An instrument layer containing a group of sublayers.
 *
 * This class is useful for gathering together a group of related
 * layers, either to hold in an external file or to work under
 * the same condition.
 */
class FGGroupLayer : public FGInstrumentLayer {
public:
  FGGroupLayer ();
  virtual ~FGGroupLayer ();
  virtual void draw ();
  // transfer pointer ownership
  virtual void addLayer (FGInstrumentLayer * const layer);
protected:
  vector <FGInstrumentLayer *> m_layers;
};

#endif
