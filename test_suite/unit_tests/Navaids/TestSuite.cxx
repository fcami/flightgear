// SPDX-FileCopyrightText: 2018 Edward d'Auvergne
// SPDX-License-Identifier: GPL-2.0-or-later

#include "test_aircraftPerformance.hxx"
#include "test_flightplan.hxx"
#include "test_fpNasal.hxx"
#include "test_navaids2.hxx"
#include "test_procedures.hxx"
#include "test_routeManager.hxx"

// Set up the unit tests.
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(FlightplanTests, "Unit tests");
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(FPNasalTests, "Unit tests");
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(NavaidsTests, "Unit tests");
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AircraftPerformanceTests, "Unit tests");
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RouteManagerTests, "Unit tests");
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ProcedureTests, "Unit tests");
