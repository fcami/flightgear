# SPDX-FileCopyrightText: Copyright (C) 2020 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    testGlobals.cxx
    NavDataCache.cxx
    PrivateAccessorFDM.cxx
    scene_graph.cxx
    TestPilot.cxx
    NasalUnitTesting_TestSuite.cxx
    TestDataLogger.cxx
    testApis.cxx
    DummyCanvasSystemAdapter.cxx
    testStringUtils.cxx
    testGlobals.hxx
    NavDataCache.hxx
    PrivateAccessorFDM.hxx
    scene_graph.hxx
    TestPilot.hxx
    TestDataLogger.hxx
    DummyCanvasSystemAdapter.hxx
    testStringUtils.hxx
)
