// SPDX-FileCopyrightText: 2016 Gaetan Allaert
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <boost/utility.hpp>
#include <stack>

#if defined (SG_MAC)
#include <OpenGL/gl.h>
#elif defined (_GLES2)
#include <GLES2/gl2.h>
#else
#include <GL/glew.h> // Must be included before <GL/gl.h>
#include <GL/gl.h>
#endif


class GL_utils : private boost::noncopyable {
public:
  static GL_utils& instance ();

  enum GLenum_Mode {
    GL_UTILS_MODELVIEW,
    GL_UTILS_PROJECTION,
    GL_UTILS_TEXTURE,
    GL_UTILS_COLOR,
    GL_UTILS_LAST,
    GL_UTILS_UNSET
  };

  GLuint load_program (const char *vert_shader_src, const char *frag_shader_src);

  void glMatrixMode (const GL_utils::GLenum_Mode mode);
  void glLoadIdentity ();
  void gluOrtho2D (const GLfloat left,
                   const GLfloat right,
                   const GLfloat bottom,
                   const GLfloat top);
  void glOrtho (const GLfloat left,
                const GLfloat right,
                const GLfloat bottom,
                const GLfloat top,
                const GLfloat nearVal,
                const GLfloat farVal);
  void glTranslatef (const GLfloat x, const GLfloat y, const GLfloat z);
  void glRotatef (const GLfloat angle, const GLfloat x, const GLfloat y, const GLfloat z);
  void glScalef (const GLfloat x, const GLfloat y, const GLfloat z);
  // C' = C X M
  void glMultMatrixf (const GLfloat m[4][4]);
  void glPushMatrix ();
  void glPopMatrix ();
  GLfloat* get_top_matrix (const GL_utils::GLenum_Mode mode);
  void Debug (const GL_utils::GLenum_Mode mode) const;

private:
  explicit GL_utils ();
  virtual ~GL_utils ();

  GLuint load_shader (GLenum type, const char *shader_src);

  typedef struct {
    GLfloat m[4][4];
  } Matrix;

  std::stack <Matrix> m_Matrix[GL_UTILS_LAST];
  GLenum_Mode m_Current_Matrix_Mode;
};
