// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FGCROPPEDTEXTURE_HXX
#define FGCROPPEDTEXTURE_HXX

#include <map>
#include <string>

#include <simgear/structure/SGSharedPtr.hxx>

#include "FGDummyTextureLoader.hxx"

using std::map;
using std::string;

/**
 * Cropped texture (should migrate out into FGFS).
 *
 * This structure wraps an SSG texture with cropping information.
 */
class FGCroppedTexture : public SGReferenced {
public:
  FGCroppedTexture (const string &path,
                    const float minX = 0.0, const float minY = 0.0,
                    const float maxX = 1.0, const float maxY = 1.0);

  virtual ~FGCroppedTexture ();

  virtual void setPath (const string &path);

  virtual const string &getPath () const;

  virtual void setCrop (const float minX, const float minY, const float maxX, const float maxY);

  static void registerTextureLoader (const string &extension,
                                     FGTextureLoaderInterface * const loader);

  virtual float getMinX () const;
  virtual float getMinY () const;
  virtual float getMaxX () const;
  virtual float getMaxY () const;
  GLuint getTexture () const;

  virtual void bind (const GLint Textured_Layer_Sampler_Loc);

private:
  string m_path;
  float m_minX, m_minY, m_maxX, m_maxY;

  GLuint m_texture;
  static GLuint s_current_bound_texture;
  static map <string, GLuint> s_cache;
  static map <string, FGTextureLoaderInterface*> s_TextureLoader;
  static FGDummyTextureLoader s_DummyTextureLoader;
};

typedef SGSharedPtr <FGCroppedTexture> FGCroppedTexture_ptr;

#endif
