// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>


// Procedure parsing and routing tests, using the synthetic EGHI procedures
// in test_data/procedures (see specs/EGHI.json).
class ProcedureTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(ProcedureTests);
    CPPUNIT_TEST(testTransitionEndingOnCommonFix);
    CPPUNIT_TEST(testTransitionJoiningPartway);
    CPPUNIT_TEST(testTransitionNotJoining);
    CPPUNIT_TEST(testSIDTransitionStartingOnCommonFix);
    CPPUNIT_TEST(testWaypointsDontInheritRestrictions);
    CPPUNIT_TEST(testHoldDefaults);
    CPPUNIT_TEST(testPBDLeg);
    CPPUNIT_TEST(testInterceptAndVectorLegs);
    CPPUNIT_TEST_SUITE_END();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    // The tests.
    void testTransitionEndingOnCommonFix();
    void testTransitionJoiningPartway();
    void testTransitionNotJoining();
    void testSIDTransitionStartingOnCommonFix();
    void testWaypointsDontInheritRestrictions();
    void testHoldDefaults();
    void testPBDLeg();
    void testInterceptAndVectorLegs();
};
