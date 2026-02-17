// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FGTEXTUREDLAYER_HXX
#define FGTEXTUREDLAYER_HXX

#include "FGCroppedTexture.hxx"
#include "FGInstrumentLayer.hxx"

/**
 * A textured layer of an instrument.
 *
 * This is a layer holding a single texture.  Normally, the texture's
 * background should be transparent so that lower layers and the panel
 * background can show through.
 */
class FGTexturedLayer : public FGInstrumentLayer {
public:
  static void Init (const GLuint Program_Object,
                    const GLint Position_Loc,
                    const GLint Tex_Coord_Loc,
                    const GLint MVP_Loc,
                    const GLint Sampler_Loc);

  FGTexturedLayer (const int w = -1, const int h = -1);
  FGTexturedLayer (const FGCroppedTexture_ptr texture, const int w = -1, const int h = -1);
  virtual ~FGTexturedLayer ();

  virtual void draw ();

  virtual void setTexture (const FGCroppedTexture_ptr texture);
  FGCroppedTexture_ptr getTexture () const;

  void setEmissive (const bool emissive);

private:
  void getDisplayList ();

  FGCroppedTexture_ptr m_texture;
  bool m_emissive;

  static GLuint Textured_Layer_Program_Object;
  static GLint Textured_Layer_Position_Loc;
  static GLint Textured_Layer_Tex_Coord_Loc;
  static GLint Textured_Layer_MVP_Loc;
  static GLint Textured_Layer_Sampler_Loc;
};

#endif
