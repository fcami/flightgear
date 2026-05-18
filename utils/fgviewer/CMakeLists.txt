# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2013 Mathias Froehlich

set(FGVIEWER_SOURCES
  ArgumentParser.cxx
  fgviewer.cxx
  Drawable.cxx
  Renderer.cxx
  SlaveCamera.cxx
  Viewer.cxx
)

add_executable(fgviewer ${FGVIEWER_SOURCES})
if(X11_FOUND)
    target_link_libraries(fgviewer ${X11_LIBRARIES})
endif()

target_link_libraries(fgviewer
        SimGearScene SimGearCore OpenGL::GL
)
install(TARGETS fgviewer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
