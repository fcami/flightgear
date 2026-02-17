// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FGPANELTRANSFORMATION_HXX
#define FGPANELTRANSFORMATION_HXX

#include <simgear/math/interpolater.hxx>
#include <simgear/props/condition.hxx>
#include <simgear/props/props.hxx>

/**
 * A transformation for a layer.
 */
class FGPanelTransformation : public SGConditional {
public:
  enum Type {
    XSHIFT,
    YSHIFT,
    ROTATION
  };

  FGPanelTransformation ();
  virtual ~FGPanelTransformation ();

  Type type;
  SGConstPropertyNode_ptr node;
  float min;
  float max;
  bool has_mod;
  float mod;
  float factor;
  float offset;
  SGInterpTable *table;
};

#endif
