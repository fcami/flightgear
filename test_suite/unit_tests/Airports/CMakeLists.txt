# SPDX-FileCopyrightText: (C) 2018 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx test_airport.cxx test_groundnet.cxx test_runway.cxx
    test_airport.hxx test_groundnet.hxx test_runway.hxx
)

fg_add_test_suite(AirportTests  u)
fg_add_test_suite(GroundnetTests u)
fg_add_test_suite(RunwayTests   u)
