// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

/**
 * Tests for JsonUriHandler — the plain HTTP GET/POST interface to the
 * property tree.  These tests exercise the synchronous request/response
 * path without any real HTTP server.
 */
class JsonUriHandlerTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(JsonUriHandlerTests);
    CPPUNIT_TEST(testGetExistingProperty);
    CPPUNIT_TEST(testGetMissingProperty);
    CPPUNIT_TEST(testPostProperty);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    // GET an existing property node → 200 with JSON value
    void testGetExistingProperty();
    // GET a path that has no property node → 404
    void testGetMissingProperty();
    // POST a new JSON value → property updated in the tree
    void testPostProperty();
};
