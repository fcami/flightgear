# SPDX-FileCopyrightText: (C) 2025  James Turner <james@flightgear.org>
# SPDX-License-Identifier: GPL-2.0-or-later

if(ENABLE_HID_INPUT)
    set(HID_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_hidinput.cxx)
    set(HID_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/test_hidinput.hxx)
endif()

set(TESTSUITE_SOURCES
    ${TESTSUITE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/TestSuite.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/test_inputDevice.cxx
    ${HID_SOURCE}
    PARENT_SCOPE
)

set(TESTSUITE_HEADERS
    ${TESTSUITE_HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_inputDevice.hxx
    ${HID_HEADER}
    PARENT_SCOPE
)
