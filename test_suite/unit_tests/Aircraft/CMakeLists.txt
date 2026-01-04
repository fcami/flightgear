# SPDX-FileCopyrightText: (C) 2025  James Turner <james@flightgear.org>
# SPDX-License-Identifier: GPL-2.0-or-later

set(TESTSUITE_SOURCES
    ${TESTSUITE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/TestSuite.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/test_controls.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/test_history.cxx
    PARENT_SCOPE
)

set(TESTSUITE_HEADERS
    ${TESTSUITE_HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_controls.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/test_history.hxx
    PARENT_SCOPE
)
