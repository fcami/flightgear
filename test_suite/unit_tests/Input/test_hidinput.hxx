// SPDX-FileCopyrightText: (C) 2017 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>


// The unit tests.
class HIDInputTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(HIDInputTests);
    CPPUNIT_TEST(testValueExtract);
    CPPUNIT_TEST(testValueInsert);
    CPPUNIT_TEST(testSignExtension);
    CPPUNIT_TEST(testDescriptorParsing);
    CPPUNIT_TEST(testConfigureWithDescriptor);
    CPPUNIT_TEST(testTM16000DescriptorParsing);
    CPPUNIT_TEST_SUITE_END();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    // The tests.
    void testValueExtract();
    void testValueInsert();
    void testSignExtension();
    void testDescriptorParsing();
    void testConfigureWithDescriptor();
    void testTM16000DescriptorParsing();
};
