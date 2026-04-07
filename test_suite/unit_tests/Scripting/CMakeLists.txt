# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: (C) 2015 Edward d'Auvergne

set(TESTSUITE_SOURCES
    ${TESTSUITE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/TestSuite.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/testNasalSys.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/testNasalLib.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/testGC.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/benchmarkPropsAccess.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/testStringFormatting.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/testVector.cxx
    PARENT_SCOPE
)

set(TESTSUITE_HEADERS
    ${TESTSUITE_HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/testNasalSys.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/testNasalLib.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/testGC.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/benchmarkPropsAccess.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/testStringFormatting.hxx
    ${CMAKE_CURRENT_SOURCE_DIR}/testVector.hxx
    PARENT_SCOPE
)
