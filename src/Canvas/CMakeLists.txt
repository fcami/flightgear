
set(SOURCES
  canvas_mgr.cxx
  FGCanvasSystemAdapter.cxx
  texture_replace.cxx
)

set(HEADERS
  canvas_mgr.hxx
  FGCanvasSystemAdapter.hxx
  gui_mgr.hxx
  texture_replace.hxx
)

# gui_mgr.cxx is compiled differently for testing
target_sources(fgfsObjects PRIVATE ${SOURCES} ${HEADERS})
set_property(GLOBAL APPEND PROPERTY FG_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gui_mgr.cxx")
