/*
 * SPDX-FileCopyrightText: (C) 2025 Torsten Dreyer
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class StringInterpolationTest : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(StringInterpolationTest);
    CPPUNIT_TEST(testInterpolation);
    CPPUNIT_TEST_SUITE_END();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    // Test
    void testInterpolation();
};
