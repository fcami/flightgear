/*
 * SPDX-FileComment: Unit tests for XML UI system
 * SPDX-FileCopyrightText: 2025 James Turner <james@flightgear.org>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>


class ItemModelTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(ItemModelTests);
    CPPUNIT_TEST(testPropertyModel);
    CPPUNIT_TEST(testNasalPropertyModel);
    CPPUNIT_TEST(testCallbacks);
    CPPUNIT_TEST(testNasalModel);
    CPPUNIT_TEST(testItemView);
    CPPUNIT_TEST(testAirportListModelSearch);
    CPPUNIT_TEST(testAirportListModelNameSearch);
    CPPUNIT_TEST(testAirportListModelRecents);

    CPPUNIT_TEST_SUITE_END();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    // The tests.
    void testCallbacks();
    void testPropertyModel();
    void testNasalPropertyModel();
    void testNasalModel();
    void testItemView();
    void testAirportListModelSearch();
    void testAirportListModelNameSearch();
    void testAirportListModelRecents();

private:
};
