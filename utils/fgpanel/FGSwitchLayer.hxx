// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FGSWITCHLAYER_HXX
#define FGSWITCHLAYER_HXX

#include "FGGroupLayer.hxx"

/**
 * A group layer that switches among its children.
 *
 * The first layer that passes its condition will be drawn, and
 * any following layers will be ignored.
 */
class FGSwitchLayer : public FGGroupLayer {
public:
  // Transfer pointers!!
  FGSwitchLayer ();
  virtual void draw ();
};

#endif
