# SPDX-FileCopyrightText: 2016 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx
    test_flightplan.cxx test_fpNasal.cxx test_navaids2.cxx
    test_aircraftPerformance.cxx test_routeManager.cxx
    test_flightplan.hxx test_fpNasal.hxx
    test_aircraftPerformance.hxx test_routeManager.hxx
)

fg_add_test_suite(FlightplanTests          u)
fg_add_test_suite(FPNasalTests             u)
fg_add_test_suite(NavaidsTests             u)
fg_add_test_suite(AircraftPerformanceTests u)
fg_add_test_suite(RouteManagerTests        u)
