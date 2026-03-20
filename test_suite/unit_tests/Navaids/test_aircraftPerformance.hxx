// SPDX-FileCopyrightText: Copyright (C) 2018 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef _FG_AIRCRAFT_PERFORMANCE_UNIT_TESTS_HXX
#define _FG_AIRCRAFT_PERFORMANCE_UNIT_TESTS_HXX


#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>


class AircraftPerformanceTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(AircraftPerformanceTests);
    CPPUNIT_TEST(testBasic);
    CPPUNIT_TEST(testAltitudeGradient);
    CPPUNIT_TEST(testLoadFromPropsNoTags);
    CPPUNIT_TEST(testLoadFromPropsTurboprop);
    CPPUNIT_TEST(testLoadFromPropsJet);
    CPPUNIT_TEST_SUITE_END();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    // The tests.
    void testBasic();
    void testAltitudeGradient();
    void testLoadFromPropsNoTags();
    void testLoadFromPropsTurboprop();
    void testLoadFromPropsJet();
};

#endif  // AircraftPerformanceTests
