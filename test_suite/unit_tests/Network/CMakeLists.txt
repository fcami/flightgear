# SPDX-FileCopyrightText: 2016 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx
    httpTestHelpers.hxx
    test_JsonUriHandler.cxx             test_JsonUriHandler.hxx
    test_PropertyChangeWebsocket.cxx    test_PropertyChangeWebsocket.hxx
    test_MirrorPropertyTreeWebsocket.cxx test_MirrorPropertyTreeWebsocket.hxx
    test_HttplibHttpd.cxx               test_HttplibHttpd.hxx
)

fg_add_test_suite(JsonUriHandlerTests              u)
fg_add_test_suite(PropertyChangeWebsocketTests     u)
fg_add_test_suite(MirrorPropertyTreeWebsocketTests u)
fg_add_test_suite(HttplibHttpdTests                u)

if(ENABLE_SWIFT)
    target_sources(fgfs_test_suite PRIVATE
        test_swiftService.cxx test_swiftAircraftManager.cxx
        test_swiftService.hxx test_swiftAircraftManager.hxx
    )
    fg_add_test_suite(SwiftAircraftManagerTest u)
    fg_add_test_suite(SwiftServiceTest         u)
endif()
