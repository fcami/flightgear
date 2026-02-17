// SPDX-FileCopyrightText: 2016 Gaetan Allaert
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <boost/utility.hpp>
#include <string>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>


class GLES_utils : private boost::noncopyable {
public:
  static GLES_utils& instance ();

  void init (const std::string &title);

  void register_display_func (void (*display_func) ());
  void register_idle_func (void (*idle_func) ());
  void register_keyboard_func (void (*keyboard_func) (unsigned char, int, int));
  void register_reshape_func (void (*reshape_func) (int, int));

  void main_loop ();

private:
  explicit GLES_utils ();
  virtual ~GLES_utils ();

  typedef struct {
#ifdef _RPI
    EGL_DISPMANX_WINDOW_T native_window;
#else
    EGLNativeWindowType native_window;
    Display *x_display;
    GLint width;
    GLint height;
#endif
    EGLint major_version;
    EGLint minor_version;
    EGLint num_configs;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLConfig config;
  } EGL_STATE_T;

  EGL_STATE_T m_State;

  void (*display_func) ();
  void (*idle_func) ();
  void (*keyboard_func) (unsigned char, int, int);
  void (*reshape_func) (int, int);

  void print_config_info (const int n, const EGLDisplay &display, EGLConfig &config);
  void init_egl (EGL_STATE_T &state, const GLuint flags);
#ifdef _RPI
  void init_dispmanx (EGL_DISPMANX_WINDOW_T &native_window);
#else
  void init_display (EGL_STATE_T &state, const std::string &title);
#endif
  GLboolean user_interrupt ();
};
