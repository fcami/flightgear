# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2026 Frederic Türpe <thefgfseagle@gmail.com>

if (UNIX AND NOT APPLE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(Pango REQUIRED IMPORTED_TARGET pango)
  if(NOT TARGET Pango::Pango)
    add_library(Pango::Pango ALIAS PkgConfig::Pango)
  endif()
else ()
  if (PANGO_LIBRARIES AND PANGO_INCLUDE_DIRS)
    # in cache already
    set(PANGO_FOUND TRUE)
    set(Pango_FOUND TRUE)
  else ()
    set(PANGO_DEFINITIONS "")

    find_path(PANGO_INCLUDE_DIR NAMES pango/pango.h PATH_SUFFIXES pango-1.0/)
    find_path(GLIB_INCLUDE_DIR NAMES glib.h PATH_SUFFIXES glib-2.0/)
    find_path(HARFBUZZ_INCLUDE_DIR NAMES hb.h PATH_SUFFIXES harfbuzz/)

    find_library(PANGO_LIBRARY NAMES pango-1.0)
    find_library(GLIB_LIBRARY NAMES glib-2.0)
    find_library(GOBJECT_LIBRARY NAMES gobject-2.0)
    find_library(GIO_LIBRARY NAMES gio-2.0)
    find_library(HARFBUZZ_LIBRARY NAMES harfbuzz)
    find_library(FRIBIDI_LIBRARY NAMES fribidi)
    find_library(PCRE2_LIBRARY NAMES pcre2-8)
    find_library(GLIB_INTL_LIBRARY NAMES intl)
    find_library(FFI_LIBRARY NAMES ffi)

      find_path(GLIB_LIB_INCLUDE_DIR NAMES glibconfig.h
        PATH_SUFFIXES glib-2.0/include lib/glib-2.0/include
      )

    if (APPLE)
      set(ICONV_LIBRARY iconv)
    endif()


    if (PANGO_INCLUDE_DIR AND GLIB_INCLUDE_DIR AND HARFBUZZ_INCLUDE_DIR AND
        PANGO_LIBRARY AND GLIB_LIBRARY AND GOBJECT_LIBRARY AND
        HARFBUZZ_LIBRARY AND GLIB_LIB_INCLUDE_DIR AND FRIBIDI_LIBRARY AND
        PCRE2_LIBRARY AND GLIB_INTL_LIBRARY AND FFI_LIBRARY
    )
      set(PANGO_INCLUDE_DIRS
        ${PANGO_INCLUDE_DIR}
        ${GLIB_INCLUDE_DIR}
        ${GLIB_LIB_INCLUDE_DIR}
        ${HARFBUZZ_INCLUDE_DIR}
      )
      set(PANGO_LIBRARIES
        ${PANGO_LIBRARY}
        ${GLIB_LIBRARY}
        ${GOBJECT_LIBRARY}
        ${HARFBUZZ_LIBRARY}
        ${FRIBIDI_LIBRARY}
        ${PCRE2_LIBRARY}
        ${GLIB_INTL_LIBRARY}
        ${FFI_LIBRARY}
      )
      set(PANGO_FOUND TRUE)
      set(Pango_FOUND TRUE)
      message(STATUS "Found Pango: ${PANGO_LIBRARY}")
    else ()
      set(PANGO_FOUND FALSE)
      set(Pango_FOUND FALSE)
      message(FATAL_ERROR "Could not find Pango")
    endif ()

    message(STATUS "Glib lib include dir: ${GLIB_LIB_INCLUDE_DIR}")

    # show the PANGO_INCLUDE_DIRS and PANGO_LIBRARIES variables only in the advanced view
    mark_as_advanced(PANGO_INCLUDE_DIRS PANGO_LIBRARIES)
  endif ()

  if(PANGO_FOUND)
    if (NOT TARGET Pango::Pango)
      add_library(GLib::GLib UNKNOWN IMPORTED)
      set_target_properties(GLib::GLib PROPERTIES
        IMPORTED_LOCATION "${GLIB_LIBRARY}"
        INTERFACE_LINK_LIBRARIES "${GLIB_INTL_LIBRARY};${PCRE2_LIBRARY};${FFI_LIBRARY};${ICONV_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GLIB_INCLUDE_DIR};${GLIB_LIB_INCLUDE_DIR}"
      )
      add_library(GObject::GObject UNKNOWN IMPORTED)
      set_target_properties(GObject::GObject PROPERTIES
        IMPORTED_LOCATION "${GOBJECT_LIBRARY}"
        INTERFACE_LINK_LIBRARIES GLib::GLib
      )
      add_library(GObject::GIO UNKNOWN IMPORTED)
      set_target_properties(GObject::GIO PROPERTIES
        IMPORTED_LOCATION "${GIO_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GLIB_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES GLib::GLib
      )
      add_library(HarfBuzz::HarfBuzz UNKNOWN IMPORTED)
      set_target_properties(HarfBuzz::HarfBuzz PROPERTIES
        IMPORTED_LOCATION "${HARFBUZZ_LIBRARY}"
        INTERFACE_LINK_LIBRARIES "${FRIBIDI_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${HARFBUZZ_INCLUDE_DIR}"
      )
      add_library(Pango::Pango UNKNOWN IMPORTED)
      set_target_properties(Pango::Pango PROPERTIES
        IMPORTED_LOCATION "${PANGO_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${PANGO_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "HarfBuzz::HarfBuzz;GObject::GObject;GObject::GIO;GLib::GLib"
      )

    endif()
  endif()
endif ()
