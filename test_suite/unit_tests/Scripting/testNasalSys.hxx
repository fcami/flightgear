// SPDX-FileCopyrightText: 2016 Edward d'Auvergne
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once


#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>


// The unit tests of the FGNasalSys subsystem.
class NasalSysTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(NasalSysTests);
    CPPUNIT_TEST(testNasalTestAPI);
    CPPUNIT_TEST(testStructEquality);
    CPPUNIT_TEST(testCommands);
    CPPUNIT_TEST(testAirportGhost);
    CPPUNIT_TEST(testCompileLarge);
    CPPUNIT_TEST(testRoundFloor);
    CPPUNIT_TEST(testRange);
    CPPUNIT_TEST(testKeywordArgInHash);
    CPPUNIT_TEST(testMemberAccess);
    CPPUNIT_TEST(testRecursiveMemberAccess);
    CPPUNIT_TEST(testNullAccess);
    CPPUNIT_TEST(testNullishChain);
    CPPUNIT_TEST(testFindComm);
    CPPUNIT_TEST(testHashDeclarationError);

    CPPUNIT_TEST_SUITE_END();

    bool checkNoNasalErrors();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    // The tests.
    void testNasalTestAPI();
    void testStructEquality();
    void testCommands();
    void testAirportGhost();
    void testCompileLarge();
    void testRoundFloor();
    void testRange();
    void testKeywordArgInHash();
    void testMemberAccess();
    void testRecursiveMemberAccess();
    void testNullAccess();
    void testNullishChain();
    void testFindComm();
    void testHashDeclarationError();
};
