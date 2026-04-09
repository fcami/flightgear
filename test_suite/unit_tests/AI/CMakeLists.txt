# SPDX-FileCopyrightText: (C) 2020  James Turner <james@flightgear.org>
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx
    test_AIFlightPlan.cxx
    test_AIManager.cxx
    test_AirportGroundRadar.cxx
    test_traffic.cxx
    test_TrafficMgr.cxx
    test_submodels.cxx
    test_VectorMath.cxx
    test_AIFlightPlan.hxx
    test_AIManager.hxx
    test_AirportGroundRadar.hxx
    test_traffic.hxx
    test_TrafficMgr.hxx
    test_submodels.hxx
    test_VectorMath.hxx
)

fg_add_test_suite(AIFlightPlanTests      u)
fg_add_test_suite(AIManagerTests         u)
fg_add_test_suite(AirportGroundRadarTests u)
fg_add_test_suite(SubmodelsTests         u)
fg_add_test_suite(VectorMathTests        u)
