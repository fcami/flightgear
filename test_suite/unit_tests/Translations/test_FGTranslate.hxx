// SPDX-FileCopyrightText: 2025 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Translations: automated tests for FGTranslate (header)
 */

#pragma once

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>

class FGTranslateTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(FGTranslateTests);
    CPPUNIT_TEST(test_defaultTranslation);
    CPPUNIT_TEST(test_en_US);
    CPPUNIT_TEST(test_fr);
    CPPUNIT_TEST(test_nonExistentTranslation);
    CPPUNIT_TEST(test_getWithDefault);
    CPPUNIT_TEST(test_pluralsAndAircraftDomain);
    CPPUNIT_TEST(test_multipleIndices);
    CPPUNIT_TEST(test_getIdsWithPrefix);
    CPPUNIT_TEST(test_addonDomain);
    CPPUNIT_TEST(test_NasalAPI_en_US);
    CPPUNIT_TEST(test_NasalAPI_fr_FR);
    CPPUNIT_TEST_SUITE_END();

public:
    // Set up function for each test.
    void setUp() {}

    // Clean up after each test.
    void tearDown();

    // The tests.
    void test_defaultTranslation();
    void test_en_US();
    void test_fr();
    void test_nonExistentTranslation();
    void test_getWithDefault();
    void test_pluralsAndAircraftDomain();
    void test_multipleIndices();
    void test_getIdsWithPrefix();
    void test_addonDomain();
    void test_NasalAPI_en_US();
    void test_NasalAPI_fr_FR();

private:
    // Subroutines called by several tests
    static void commonBetweenDefaultTranslationAndEn_US();
    static void NasalAPI_languageIndependentTests();
};
