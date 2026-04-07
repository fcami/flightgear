// SPDX-FileCopyrightText: 2026 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Automated tests for the Nasal format() function
 */

#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class NasalStringFormattingTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(NasalStringFormattingTests);
    CPPUNIT_TEST(test_builtinTypes);
    CPPUNIT_TEST_SUITE_END();

public:
    // Set up function for each test.
    void setUp() {}

    // Clean up after each test.
    void tearDown();

    // The tests.
    void test_builtinTypes();
};
