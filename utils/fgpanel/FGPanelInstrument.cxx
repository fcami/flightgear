// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#include "FGPanelInstrument.hxx"

FGPanelInstrument::FGPanelInstrument () {
  setPosition (0, 0);
  setSize (0, 0);
}

FGPanelInstrument::FGPanelInstrument (const int x, const int y, const int w, const int h) {
  setPosition (x, y);
  setSize (w, h);
}

FGPanelInstrument::~FGPanelInstrument () {
}

void
FGPanelInstrument::setPosition (const int x, const int y) {
  m_x = x;
  m_y = y;
}

void
FGPanelInstrument::setSize (const int w, const int h) {
  m_w = w;
  m_h = h;
}

int
FGPanelInstrument::getXPos () const {
  return m_x;
}

int
FGPanelInstrument::getYPos () const {
  return m_y;
}

int
FGPanelInstrument::getWidth () const {
  return m_w;
}

int
FGPanelInstrument::getHeight () const {
  return m_h;
}
