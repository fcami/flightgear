# SPDX-FileCopyrightText: 2026 Florent Rougon
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileComment: Set up variables pertaining to the 'ccache' compiler cache

# If any of these variables is set, we assume the user knows what they are
# doing and don't modify them.
if(CMAKE_C_COMPILER_LAUNCHER OR CMAKE_CXX_COMPILER_LAUNCHER)
  if(CMAKE_C_COMPILER_LAUNCHER)
    message(STATUS
      "CMAKE_C_COMPILER_LAUNCHER is set to ${CMAKE_C_COMPILER_LAUNCHER}.")
  else()
    message(STATUS "CMAKE_C_COMPILER_LAUNCHER is not set.")
  endif()

  if(CMAKE_CXX_COMPILER_LAUNCHER)
    message(STATUS
      "CMAKE_CXX_COMPILER_LAUNCHER is set to ${CMAKE_CXX_COMPILER_LAUNCHER}.")
  else()
    message(STATUS "CMAKE_CXX_COMPILER_LAUNCHER is not set.")
  endif()
elseif(ENABLE_CCACHE)
  find_program(CCACHE_PROGRAM ccache)

  if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER ccache)
    set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
    message(STATUS "Compiler cache 'ccache' is enabled (${CCACHE_PROGRAM}).")
  else()
    message(STATUS
      "Compiler cache 'ccache' is disabled because the executable wasn't found.")
  endif()
else()
    message(STATUS "Compiler cache 'ccache' is disabled.")
endif()
