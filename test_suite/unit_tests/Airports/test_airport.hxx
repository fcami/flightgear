/*
 * SPDX-FileCopyrightText: 2021 Keith Paterson
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <memory>

#include <simgear/props/props.hxx>

class SGGeod;

// The flight plan unit tests.
class AirportTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(AirportTests);
    CPPUNIT_TEST(testAirport);
    CPPUNIT_TEST(testAirportNasal);
    CPPUNIT_TEST_SUITE_END();


public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    // The tests.
    void testAirport();
    void testAirportNasal();
};
