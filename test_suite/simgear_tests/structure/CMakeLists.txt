# SPDX-FileCopyrightText: 2020  James Turner <james@flightgear.org>
# SPDX-License-Identifier: GPL-2.0-or-later

set(TESTSUITE_SOURCES
    ${TESTSUITE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/TestSuite.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/test_binding.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/test_event.cxx
    PARENT_SCOPE
)

set(TESTSUITE_HEADERS
    ${TESTSUITE_HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_binding.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/test_event.hxx
    PARENT_SCOPE
)
