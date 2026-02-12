// SPDX-FileCopyrightText: 2018 Edward d'Auvergne
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FG_FLIGHTPLAN_UNIT_TESTS_HXX
#define FG_FLIGHTPLAN_UNIT_TESTS_HXX


#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>


// The flight plan unit tests.
class FlightplanTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(FlightplanTests);
    CPPUNIT_TEST(testBasic);
    CPPUNIT_TEST(testRoutePathBasic);
    CPPUNIT_TEST(testRoutePathSkipped);
    CPPUNIT_TEST(testRoutePathTrivialFlightPlan);
    CPPUNIT_TEST(testBasicAirways);
    CPPUNIT_TEST(testAirwayNetworkRoute);
    CPPUNIT_TEST(testBug1814);
    CPPUNIT_TEST(testRoutPathWpt0Midflight);
    CPPUNIT_TEST(testRoutePathVec);
    CPPUNIT_TEST(testRoutePathFinalLegVQPR15);
    CPPUNIT_TEST(testLoadSaveMachRestriction);
    CPPUNIT_TEST(testOnlyDiscontinuityRoute);
    CPPUNIT_TEST(testBasicDiscontinuity);
    CPPUNIT_TEST(testLeadingWPDynamic);
    CPPUNIT_TEST(testRadialIntercept);
    CPPUNIT_TEST(loadFGFPWithoutDepartureArrival);
    CPPUNIT_TEST(loadFGFPWithEmbeddedProcedures);
    CPPUNIT_TEST(loadFGFPWithOldProcedures);
    CPPUNIT_TEST(loadFGFPWithProcedureIdents);
    CPPUNIT_TEST(testCloningBasic);
    CPPUNIT_TEST(testCloningFGFP);
    CPPUNIT_TEST(testCloningProcedures);
    CPPUNIT_TEST(testBug2616);
    CPPUNIT_TEST(testRoute);
    CPPUNIT_TEST(testViaInsertIntoFP);
    CPPUNIT_TEST(testViaInsertIntoRoute);
    CPPUNIT_TEST(loadFGFPAsRoute);
    CPPUNIT_TEST(testLoadSaveBetweenRestriction);
    CPPUNIT_TEST(testRestrictionUnits);
    CPPUNIT_TEST(testDeleteProcedureWaypoint);

    //  CPPUNIT_TEST(testParseICAORoute);
    // CPPUNIT_TEST(testParseICANLowLevelRoute);
    CPPUNIT_TEST_SUITE_END();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    // The tests.
    void testBasic();
    void testRoutePathBasic();
    void testRoutePathSkipped();
    void testRoutePathTrivialFlightPlan();
    void testBasicAirways();
    void testAirwayNetworkRoute();
    void testParseICAORoute();
    void testParseICANLowLevelRoute();
    void testBug1814();
    void testRoutPathWpt0Midflight();
    void testRoutePathVec();
    void testRoutePathFinalLegVQPR15();
    void testLoadSaveMachRestriction();
    void testBasicDiscontinuity();
    void testOnlyDiscontinuityRoute();
    void testLeadingWPDynamic();
    void testRadialIntercept();
    void loadFGFPWithoutDepartureArrival();
    void loadFGFPWithEmbeddedProcedures();
    void loadFGFPWithOldProcedures();
    void loadFGFPWithProcedureIdents();
    void testCloningBasic();
    void testCloningFGFP();
    void testCloningProcedures();
    void testBug2616();
    void testRoute();
    void testViaInsertIntoFP();
    void testViaInsertIntoRoute();
    void loadFGFPAsRoute();
    void testLoadSaveBetweenRestriction();
    void testRestrictionUnits();
    void testDeleteProcedureWaypoint();
};

#endif  // FG_FLIGHTPLAN_UNIT_TESTS_HXX
