// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "FGCroppedTexture.hxx"
#include "FGInstrumentLayer.hxx"
#include "FGPanelInstrument.hxx"


/**
 * An instrument constructed of multiple layers.
 *
 * Each individual layer can be rotated or shifted to correspond
 * to internal FGFS instrument readings.
 */
class FGLayeredInstrument : public FGPanelInstrument {
public:
  FGLayeredInstrument (const int x, const int y, const int w, const int h);
  virtual ~FGLayeredInstrument ();

  virtual void draw ();

  // Transfer pointer ownership!!
  virtual int addLayer (FGInstrumentLayer * const layer);
  virtual int addLayer (const FGCroppedTexture_ptr texture, const int w = -1, const int h = -1);

  // Transfer pointer ownership!!
  virtual void addTransformation (FGPanelTransformation * const transformation);

private:
  typedef std::vector <FGInstrumentLayer *> layer_list;
  layer_list m_layers;
};
