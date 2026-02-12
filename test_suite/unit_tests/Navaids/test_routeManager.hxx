// SPDX-FileCopyrightText: 2019 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef _FG_ROUTE_MANAGER_UNIT_TESTS_HXX
#define _FG_ROUTE_MANAGER_UNIT_TESTS_HXX


#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>

class SGGeod;
class GPS;

// The flight plan unit tests.
class RouteManagerTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(RouteManagerTests);
    CPPUNIT_TEST(testBasic);
    CPPUNIT_TEST(testDefaultSID);
    CPPUNIT_TEST(testDefaultApproach);
    CPPUNIT_TEST(testDirectToLegOnFlightplanAndResume);
    CPPUNIT_TEST(testHoldFromNasal);
    CPPUNIT_TEST(testSequenceDiscontinuityAndResume);
    CPPUNIT_TEST(testHiddenWaypoints);
    CPPUNIT_TEST(loadGPX);
    CPPUNIT_TEST(loadFGFP);
    CPPUNIT_TEST(testRouteWithProcedures);
    CPPUNIT_TEST(testRouteWithApproachProcedures);
    CPPUNIT_TEST(testsSelectNavaid);
    CPPUNIT_TEST(testCommandAPI);
    CPPUNIT_TEST(testRMBug2616);
    CPPUNIT_TEST(testsSelectWaypoint);
    CPPUNIT_TEST(testsSelectWaypoint2);
    CPPUNIT_TEST(testAppendWaypoint);
    CPPUNIT_TEST(testEditProcedures);

    CPPUNIT_TEST_SUITE_END();

   // void setPositionAndStabilise(FGNavRadio* r, const SGGeod& g);

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    void setPositionAndStabilise(const SGGeod& g);

    // The tests.
    void testBasic();
    void testDefaultSID();
    void testDefaultApproach();
    void testDirectToLegOnFlightplanAndResume();
    void testHoldFromNasal();
    void testSequenceDiscontinuityAndResume();
    void testHiddenWaypoints();
    void loadGPX();
    void loadFGFP();
    void testRouteWithProcedures();
    void testRouteWithApproachProcedures();
    void testsSelectNavaid();
    void testCommandAPI();
    void testsSelectWaypoint();
    void testRMBug2616();
    void testsSelectWaypoint2();
    void testAppendWaypoint();
    void testEditProcedures();

private:
    GPS* m_gps = nullptr;
};

#endif  // _FG_ROUTE_MANAGER_UNIT_TESTS_HXX
