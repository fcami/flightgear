// SPDX-FileCopyrightText: 2018 Edward d'Auvergne
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once


#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <simgear/structure/subsystem_mgr.hxx>

class FGNavRadio;
class SGGeod;

// The flight plan unit tests.
class CommRadioTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(CommRadioTests);

    CPPUNIT_TEST(testBasic);
    CPPUNIT_TEST(testEightPointThree);
    CPPUNIT_TEST(testEPLLTuning833);
    CPPUNIT_TEST(testEPLLTuning25);
    CPPUNIT_TEST(testFullDuplex);

    CPPUNIT_TEST_SUITE_END();

    SGSubsystemRef setupStandardRadio(const std::string& name, int index, bool enable833);

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    //  std::string formatFrequency(double f);

    // The tests.
    void testBasic();
    void testEightPointThree();
    void testEPLLTuning833();
    void testEPLLTuning25();
    void testFullDuplex();
};
