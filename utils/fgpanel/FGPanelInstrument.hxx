// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <simgear/props/condition.hxx>


/**
 * Abstract base class for a panel instrument.
 *
 * A panel instrument consists of zero or more actions, associated
 * with mouse clicks in rectangular areas.  Currently, the only
 * concrete class derived from this is FGLayeredInstrument, but others
 * may show up in the future (some complex instruments could be
 * entirely hand-coded, for example).
 */
class FGPanelInstrument : public SGConditional {
public:
  FGPanelInstrument ();
  FGPanelInstrument (const int x, const int y, const int w, const int h);
  virtual ~FGPanelInstrument ();

  virtual void draw () = 0;

  virtual void setPosition (const int x, const int y);
  virtual void setSize (const int w, const int h);

  virtual int getXPos () const;
  virtual int getYPos () const;
  virtual int getWidth () const;
  virtual int getHeight () const;

private:
  int m_x, m_y, m_w, m_h;
};
