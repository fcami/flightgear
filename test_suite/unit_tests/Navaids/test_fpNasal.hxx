// SPDX-FileCopyrightText: 2018 Edward d'Auvergne
// SPDX-License-Identifier: GPL-2.0-or-later


#pragma once

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>


// The flight plan unit tests.
class FPNasalTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(FPNasalTests);
    CPPUNIT_TEST(testBasic);
    CPPUNIT_TEST(testRestrictions);
    CPPUNIT_TEST(testSegfaultWaypointGhost);
    CPPUNIT_TEST(testSIDTransitionAPI);
    CPPUNIT_TEST(testSTARTransitionAPI);
    CPPUNIT_TEST(testApproachTransitionAPI);
    CPPUNIT_TEST(testApproachTransitionAPIWithCloning);
    CPPUNIT_TEST(testAirwaysAPI);
    CPPUNIT_TEST(testTotalDistanceAPI);
    CPPUNIT_TEST(testRunwayMagVar);

    CPPUNIT_TEST_SUITE_END();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    // The tests.
    void testBasic();
    void testRestrictions();
    void testSegfaultWaypointGhost();
    void testSIDTransitionAPI();
    void testSTARTransitionAPI();
    void testApproachTransitionAPI();
    void testApproachTransitionAPIWithCloning();
    void testAirwaysAPI();
    void testTotalDistanceAPI();
    void testRunwayMagVar();
};
