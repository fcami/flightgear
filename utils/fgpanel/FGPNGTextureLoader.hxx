// SPDX-FileCopyrightText: 2011 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __FGPNGTEXTURELOADER_HXX
#define __FGPNGTEXTURELOADER_HXX

#include "FGTextureLoaderInterface.hxx"
using std::string;

class FGPNGTextureLoader : public FGTextureLoaderInterface {
public:
  virtual GLuint loadTexture (const string &filename);

  const static GLuint NOTEXTURE = 0;
};

#endif
