# SPDX-FileCopyrightText: 2016 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE TestSuite.cxx)

if(ENABLE_SWIFT)
    target_sources(fgfs_test_suite PRIVATE
        test_swiftService.cxx test_swiftAircraftManager.cxx
        test_swiftService.hxx test_swiftAircraftManager.hxx
    )
    fg_add_test_suite(SwiftAircraftManagerTest u)
    fg_add_test_suite(SwiftServiceTest         u)
endif()
