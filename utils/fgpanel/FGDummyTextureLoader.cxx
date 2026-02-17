// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#include "FGDummyTextureLoader.hxx"
#include <string>
using std::string;

GLuint
FGDummyTextureLoader::loadTexture (const string& filename) {
  GLuint texture;
  glGenTextures (1, &texture);
  glBindTexture (GL_TEXTURE_2D, texture);

  GLubyte image[ 2 * 2 * 3 ];

  /* Red and white chequerboard */
  image [ 0] = 255; image [ 1] =   0; image [ 2] =   0;
  image [ 3] = 255; image [ 4] = 255; image [ 5] = 255;
  image [ 6] = 255; image [ 7] = 255; image [ 8] = 255;
  image [ 9] = 255; image [10] =   0; image [11] =   0;

  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0,
                GL_RGB, GL_UNSIGNED_BYTE, (GLvoid*) image);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  return texture;
}
