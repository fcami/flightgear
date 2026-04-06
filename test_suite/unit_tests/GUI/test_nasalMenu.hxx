/*
 * SPDX-FileComment: Unit tests for NasalMenu / FGNasalMenuBar
 * SPDX-FileCopyrightText: 2026 James Turner <james@flightgear.org>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>


class NasalMenuTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(NasalMenuTests);
    CPPUNIT_TEST(testBasicStructure);
    CPPUNIT_TEST(testUpdateCallback);
    CPPUNIT_TEST(testDynamicItemAddRemove);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    void testBasicStructure();
    void testUpdateCallback();
    void testDynamicItemAddRemove();

private:
};
