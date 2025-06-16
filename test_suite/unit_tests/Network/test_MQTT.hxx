/*
 * SPDX-FileCopyrightText: (C) 2025 Torsten Dreyer
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class MQTTTest : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(MQTTTest);

    CPPUNIT_TEST(testPropertyPublisherNonrecursive);
    CPPUNIT_TEST(testPropertyPublisherRecursive);
    CPPUNIT_TEST(testTemplatePublisher);
    CPPUNIT_TEST(testPropertySubscription);
    CPPUNIT_TEST(testTopicFilterMatcher);
    CPPUNIT_TEST(testTopicFilterValidator);


    CPPUNIT_TEST_SUITE_END();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    // Test
    void testTopicFilterValidator();
    void testTopicFilterMatcher();
    void testPropertyPublisherNonrecursive();
    void testPropertyPublisherRecursive();
    void testTemplatePublisher();
    void testPropertySubscription();
};
