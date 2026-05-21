/*
 * SPDX-FileCopyrightText: 2016 Edward d'Auvergne
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once


#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>

#include <simgear/props/props.hxx>


// The unit tests of the simgear property tree.
class SimgearPropsTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(SimgearPropsTests);
    CPPUNIT_TEST(testAliasLeak);
    CPPUNIT_TEST(testPropsCopyIf);
    CPPUNIT_TEST(testDoubleAlias);
    CPPUNIT_TEST(testPropsXMLSourceLocation);
    CPPUNIT_TEST_SUITE_END();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    // The tests.
    void testAliasLeak();
    void testPropsCopyIf();
    void testDoubleAlias();
    void testPropsXMLSourceLocation();

private:
    // A property tree.
    SGPropertyNode *tree;
};
