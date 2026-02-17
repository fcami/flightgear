// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <simgear/timing/timestamp.hxx>

#include "FGFontCache.hxx"
#include "FGInstrumentLayer.hxx"


/**
 * A text layer of an instrument.
 *
 * This is a layer holding a string of static and/or generated text.
 * It is useful for instruments that have text displays, such as
 * a chronometer, GPS, or NavCom radio.
 */

class FGTextLayer : public FGInstrumentLayer {
public:
  enum ChunkType {
    TEXT,
    TEXT_VALUE,
    DOUBLE_VALUE
  };

  class Chunk : public SGConditional {
  public:
    Chunk (const std::string &text,
           const std::string &fmt = "%s");
    Chunk (const ChunkType type,
           const SGPropertyNode *node,
           const std::string &fmt = "",
           const float mult = 1.0,
           const float offs = 0.0,
           const bool truncation = false);

    const char *getValue () const;
  private:
    ChunkType m_type;
    std::string m_text;
    SGConstPropertyNode_ptr m_node;
    std::string m_fmt;
    float m_mult;
    float m_offs;
    bool m_trunc;
    mutable char m_buf[1024];

  };

  static bool Init ();

  FGTextLayer (const int w = -1, const int h = -1);
  virtual ~FGTextLayer ();

  virtual void draw ();

  // Transfer pointer!!
  virtual void addChunk (Chunk * const chunk);
  virtual void setColor (const float r,
                         const float g,
                         const float b);
  virtual void setPointSize (const float size);
  virtual void setFontName (const std::string &name);

private:

  void recalc_value () const;

  typedef std::vector<Chunk *> chunk_list;
  chunk_list m_chunks;
  float m_color[4];

  float m_pointSize;
  static SGPath The_Font_Path;
  mutable std::string m_font_name;
  mutable std::string m_value;
  mutable SGTimeStamp m_then;
  mutable SGTimeStamp m_now;

  static FGFontCache The_Font_Cache;

  static GLuint Text_Layer_Program_Object;
  static GLint Text_Layer_Position_Loc;
  static GLint Text_Layer_Tex_Coord_Loc;
  static GLint Text_Layer_MVP_Loc;
  static GLint Text_Layer_Sampler_Loc;
  static GLint Text_Layer_Color_Loc;
};
