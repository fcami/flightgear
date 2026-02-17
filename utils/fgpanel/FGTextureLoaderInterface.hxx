// SPDX-FileCopyrightText: 2011 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#if defined (SG_MAC)
#include <OpenGL/gl.h>
#elif defined (_GLES2)
#include <GLES2/gl2.h>
#else
#include <GL/glew.h> // Must be included before <GL/gl.h>
#include <GL/gl.h>
#endif

#include <simgear/compiler.h>


class FGTextureLoaderInterface {
public:
  virtual GLuint loadTexture (const std::string &filename) = 0;
};
