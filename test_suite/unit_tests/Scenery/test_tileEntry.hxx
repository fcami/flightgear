/*
 * SPDX-FileCopyrightText: 2026 James Turner <james@flightgear.org>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TileEntryTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(TileEntryTests);
    CPPUNIT_TEST(testNoTerraSyncNoData);
    CPPUNIT_TEST(testTerraSyncDisabledNoData);
    CPPUNIT_TEST(testTerraSyncEnabledNoData);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    // No SGTerraSync subsystem registered at all, and no VPB data on disk:
    // should fall back to the Ocean placeholder, not get stuck waiting.
    void testNoTerraSyncNoData();

    // SGTerraSync registered but disabled, and no VPB data on disk: same as
    // above, this is the actual regression from #3539.
    void testTerraSyncDisabledNoData();

    // SGTerraSync registered and enabled, no VPB data on disk yet: tile
    // should wait for TerraSync (AwaitingSync), not jump to Ocean.
    void testTerraSyncEnabledNoData();
};
