/*
SPDX-FileCopyrightText: 2026 James Turner
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <simgear/props/props.hxx>
#include <simgear/structure/SGBinding.hxx>

// The unit tests of the simgear property tree.
class SimgearBindingTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(SimgearBindingTests);
    CPPUNIT_TEST(testRegularBinding);
    CPPUNIT_TEST(testExpressionBinding);
    CPPUNIT_TEST_SUITE_END();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();


    // The tests.
    void testRegularBinding();
    void testExpressionBinding();

private:
};
