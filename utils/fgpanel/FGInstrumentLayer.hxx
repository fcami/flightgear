// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>
#include <simgear/props/condition.hxx>

#include "FGPanelTransformation.hxx"


/**
 * A single layer of a multi-layered instrument.
 *
 * Each layer can be subject to a series of transformations based
 * on current FGFS instrument readings: for example, a texture
 * representing a needle can rotate to show the airspeed.
 */
class FGInstrumentLayer : public SGConditional {
public:
  FGInstrumentLayer (const int w = -1, const int h = -1);
  virtual ~FGInstrumentLayer ();

  virtual void draw () = 0;
  virtual void transform () const;

  virtual int getWidth () const;
  virtual int getHeight () const;
  virtual void setWidth (const int w);
  virtual void setHeight (const int h);

  // Transfer pointer ownership!!
  // DEPRECATED
  virtual void addTransformation (FGPanelTransformation * const transformation);

protected:
  int m_w, m_h;

  typedef std::vector <FGPanelTransformation *> transformation_list;
  transformation_list m_transformations;
};
