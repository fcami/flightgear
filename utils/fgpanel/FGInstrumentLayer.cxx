// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#include <math.h>
#include <simgear/props/props.hxx>

#include "GL_utils.hxx"
#include "FGInstrumentLayer.hxx"

FGInstrumentLayer::FGInstrumentLayer (const int w, const int h) :
  m_w (w), m_h (h) {
}

FGInstrumentLayer::~FGInstrumentLayer () {
  for (transformation_list::iterator it = m_transformations.begin ();
       it != m_transformations.end ();
       ++it) {
    delete *it;
    *it = 0;
  }
}

void
FGInstrumentLayer::transform () const {
  for (transformation_list::const_iterator it = m_transformations.begin ();
       it != m_transformations.end ();
       ++it) {
    FGPanelTransformation *t = *it;
    if (t->test ()) {
      float val (t->node == 0 ? 0.0 : t->node->getFloatValue ());
      if (t->has_mod) {
          val = fmod (val, t->mod);
      }
      if (val < t->min) {
        val = t->min;
      } else if (val > t->max) {
        val = t->max;
      }

      if (t->table == 0) {
        val = val * t->factor + t->offset;
      } else {
        val = t->table->interpolate (val) * t->factor + t->offset;
      }

      switch (t->type) {
      case FGPanelTransformation::XSHIFT:
        GL_utils::instance ().glTranslatef (val, 0.0, 0.0);
        break;
      case FGPanelTransformation::YSHIFT:
        GL_utils::instance ().glTranslatef (0.0, val, 0.0);
        break;
      case FGPanelTransformation::ROTATION:
        GL_utils::instance ().glRotatef (-val, 0.0, 0.0, 1.0);
        break;
      }
    }
  }
}

int
FGInstrumentLayer::getWidth () const {
  return m_w;
}

int
FGInstrumentLayer::getHeight () const {
  return m_h;
}

void
FGInstrumentLayer::setWidth (const int w) {
  m_w = w;
}

void
FGInstrumentLayer::setHeight (const int h) {
  m_h = h;
}

void
FGInstrumentLayer::addTransformation (FGPanelTransformation * const transformation) {
  m_transformations.push_back (transformation);
}
