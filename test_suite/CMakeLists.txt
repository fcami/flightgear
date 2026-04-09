# SPDX-FileCopyrightText: 2016 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

# CMake module includes.
include(SetupFGFSEmbeddedResources)
include(SetupFGFSIncludes)
include(SetupFGFSLibraries)

#-----------------------------------------------------------------------------
# Register a CppUnit test suite with ctest.
# SUITE_NAME must match the first argument of CPPUNIT_TEST_SUITE_NAMED_REGISTRATION
# in the corresponding TestSuite.cxx.
# TYPE is the single-letter ctest category flag:
#   u  unit tests,  s  system tests,  m  simgear unit tests
function(fg_add_test_suite SUITE_NAME TYPE)
    add_test(NAME "${SUITE_NAME}"
             COMMAND fgfs_test_suite --ctest -${TYPE} "${SUITE_NAME}")
endfunction()

# The test suite output directory.
set(TESTSUITE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Get the CppUnit sources and headers to be built into the test suite.
if (NOT SYSTEM_CPPUNIT)
    message(STATUS "CppUnit: Building the FlightGear supplied CppUnit library")

    get_property(CPPUNIT_SOURCES GLOBAL PROPERTY CPPUNIT_SOURCES)
    get_property(CPPUNIT_HEADERS GLOBAL PROPERTY CPPUNIT_HEADERS)

    add_library(CppUnitLib STATIC ${CPPUNIT_SOURCES} ${CPPUNIT_HEADERS})

    target_include_directories(CppUnitLib PUBLIC "${PROJECT_SOURCE_DIR}/3rdparty/cppunit/include")
elseif(NOT TARGET CppUnitLib)
    message(FATAL_ERROR "CppUnit: system CppUnit library not found correctly")
else()
    message(STATUS "CppUnit: Linking to the system supplied CppUnit library")
endif()

# most object files come from fgfsObjects target: this property
# contains all the sources which need to be compiled separately
# for the test-suite, because they depend on BUILDING_TESTSUITE define
# we setup below via target_compile_definitions
get_property(TEST_SOURCES GLOBAL PROPERTY FG_TEST_SOURCES)

#-----------------------------------------------------------------------------
# Set up the binary.

# Set up the embedded resources.
setup_fgfs_embedded_resources()

# Set up the separate executable for running the test suite.
add_executable(fgfs_test_suite
    ${TEST_SOURCES}
    bootstrap.cxx
    dataStore.cxx
    fgCompilerOutputter.cxx
    fgJunitOutputter.cxx
    fgTestListener.cxx
    fgTestRunner.cxx
    formatting.cxx
    logging.cxx
    testSuite.cxx
)

target_link_libraries(fgfs_test_suite PRIVATE fgfsObjects)

add_dependencies(fgfs_test_suite buildId)
set_target_properties(fgfs_test_suite
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TESTSUITE_OUTPUT_DIR}"
)

# Set up the target links.
setup_fgfs_libraries(fgfs_test_suite)
# Additional search paths for includes.
setup_fgfs_includes(fgfs_test_suite)

# test code includes its headers as test_suite/foo.h, so we need this
target_include_directories(fgfs_test_suite PUBLIC ${PROJECT_SOURCE_DIR})

target_compile_definitions(fgfs_test_suite PUBLIC BUILDING_TESTSUITE)

# Additional libraries just for the test suite.
target_link_libraries(fgfs_test_suite PRIVATE CppUnitLib)

if (MSVC)
    file(TO_NATIVE_PATH "${FG_QT_BIN_DIR}" _qt_bin_dir_native)
    file(TO_NATIVE_PATH "${FINAL_MSVC_3RDPARTY_DIR}/bin" _msvc_3rdparty_bin_dir)
    file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}" _install_bin_dir)

    set(CMAKE_MSVCIDE_RUN_PATH "${_install_bin_dir};${_msvc_3rdparty_bin_dir};${_qt_bin_dir_native}")
endif()

if (FG_QT_ROOT_DIR)
    # for QtPlatformHeaders
    target_include_directories(fgfs_test_suite PRIVATE ${FG_QT_ROOT_DIR}/include)
endif()

#-----------------------------------------------------------------------------
# Add each test suite category.
# Each subdirectory uses fg_add_test_suite() to register its sources with the
# fgfs_test_suite target and to add ctest entries.

add_subdirectory(FGTestApi)

foreach(test_category
        gui_tests
        simgear_tests
        system_tests
        unit_tests
        fgdata_tests
    )
    add_subdirectory(${test_category})
endforeach()

#-----------------------------------------------------------------------------
# target to run the tests
if(ENABLE_AUTOTESTING)
    set(TEST_SUITE_COMMAND "fgfs_test_suite")
    set(TEST_SUITE_COMMENT "Running the full FlightGear test suite")
else()
    set(TEST_SUITE_COMMENT "Building the FlightGear test suite.")
endif(ENABLE_AUTOTESTING)

add_custom_target(test_suite
    ${TEST_SUITE_COMMAND}
    DEPENDS fgfs_test_suite
    COMMENT ${TEST_SUITE_COMMENT}
)
