// SPDX-FileCopyrightText: 2011 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __FGPANELAPPLICATION_HXX
#define __FGPANELAPPLICATION_HXX

#include <simgear/structure/subsystem_mgr.hxx>

#include "FGGLApplication.hxx"
#include "FGPanel.hxx"
#include "FGPanelProtocol.hxx"

class FGPanelApplication : public FGGLApplication {
public:
  FGPanelApplication (int argc, char **argv);
  ~FGPanelApplication ();

  void Run ();

protected:
  virtual void Key (const unsigned char key, const int x, const int y);
  virtual void Idle ();
  // !!! virtual void Display ();
  virtual void Reshape (const int width, const int height);

  virtual void Init ();

  double Sleep ();

  SGSharedPtr<FGPanel> panel;
  SGSharedPtr<FGPanelProtocol> protocol;

  int width;
  int height;
};

#endif
