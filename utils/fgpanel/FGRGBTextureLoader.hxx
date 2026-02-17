// SPDX-FileCopyrightText: 2011 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __FGRGBTEXTURELOADER_HXX
#define __FGRGBTEXTURELOADER_HXX

#include "FGTextureLoaderInterface.hxx"
using std::string;

class FGRGBTextureLoader : public FGTextureLoaderInterface {
public:
  virtual GLuint loadTexture (const string &filename);

  const static GLuint NOTEXTURE = 0;
};

#endif
