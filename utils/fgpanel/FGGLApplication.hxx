// SPDX-FileCopyrightText: 2011 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __FGGLAPPLICATION_HXX
#define __FGGLAPPLICATION_HXX

class FGGLApplication {
public:
  FGGLApplication (const char *a_name, int argc, char **argv);
  virtual ~FGGLApplication ();
  void Run(const int glutMode,
           const bool gameMode,
           int width = -1,
           int height = -1,
           const int bpp = 32);

  protected:
  virtual void Key (const unsigned char key, const int x, const int y) {}
  virtual void Idle () {}
  virtual void Display () {}
  virtual void Reshape (const int width, const int height) {}

  virtual void Init () {}

  int windowId;
  bool gameMode;

  const char *name;

  static FGGLApplication *application;
private:
  static void KeyCallback (const unsigned char key, const int x, const int y);
  static void IdleCallback ();
  static void DisplayCallback ();
  static void ReshapeCallback (const int width, const int height);

};

#endif
