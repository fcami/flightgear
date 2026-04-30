# SPDX-FileCopyrightText: 2013 Thomas Geymayer
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfsObjects PRIVATE
  canvas_mgr.cxx
  FGCanvasSystemAdapter.cxx
  texture_replace.cxx
  canvas_mgr.hxx
  FGCanvasSystemAdapter.hxx
  gui_mgr.hxx
  texture_replace.hxx
)
set_property(GLOBAL APPEND PROPERTY FG_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gui_mgr.cxx")
