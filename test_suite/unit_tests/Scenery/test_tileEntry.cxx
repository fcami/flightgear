/*
 * SPDX-FileCopyrightText: 2026 James Turner <james@flightgear.org>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "test_tileEntry.hxx"

#include "test_suite/FGTestApi/testGlobals.hxx"

#include <simgear/bucket/newbucket.hxx>
#include <simgear/scene/material/matlib.hxx>
#include <simgear/scene/tsync/terrasync.hxx>
#include <simgear/scene/util/SGReaderWriterOptions.hxx>

#include <Main/globals.hxx>
#include <Scenery/tileentry.hxx>

using namespace simgear;

namespace {

osg::ref_ptr<SGReaderWriterOptions> makeOptionsWithNoScenery()
{
    auto options = new SGReaderWriterOptions;
    // Deliberately empty: nothing on disk for checkForVPBData() to find.
    options->setMaterialLib(new SGMaterialLib);
    return options;
}

} // namespace

void TileEntryTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("tileEntry");
}

void TileEntryTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}

void TileEntryTests::testNoTerraSyncNoData()
{
    // No SGTerraSync subsystem registered at all.
    SGBucket bucket(SGGeod::fromDeg(-3.352, 55.943));
    auto options = makeOptionsWithNoScenery();

    VPBTileEntry tile(bucket, options);

    CPPUNIT_ASSERT_EQUAL(VPBTileEntry::TileState::OceanData, tile.getTileState());
    CPPUNIT_ASSERT(tile.is_loaded());
}

void TileEntryTests::testTerraSyncDisabledNoData()
{
    auto terraSync = globals->get_subsystem_mgr()->add<SGTerraSync>();
    terraSync->setRoot(globals->get_props());
    terraSync->bind();
    // /sim/terrasync/enabled defaults to false; leave it that way.

    SGBucket bucket(SGGeod::fromDeg(-3.352, 55.943));
    auto options = makeOptionsWithNoScenery();

    VPBTileEntry tile(bucket, options);

    CPPUNIT_ASSERT_EQUAL(VPBTileEntry::TileState::OceanData, tile.getTileState());
    CPPUNIT_ASSERT(tile.is_loaded());
}

void TileEntryTests::testTerraSyncEnabledNoData()
{
    auto terraSync = globals->get_subsystem_mgr()->add<SGTerraSync>();
    terraSync->setRoot(globals->get_props());
    terraSync->bind();
    globals->get_props()->getNode("/sim/terrasync/enabled", true)->setBoolValue(true);

    SGBucket bucket(SGGeod::fromDeg(-3.352, 55.943));
    auto options = makeOptionsWithNoScenery();

    VPBTileEntry tile(bucket, options);

    CPPUNIT_ASSERT_EQUAL(VPBTileEntry::TileState::AwaitingSync, tile.getTileState());
    // still showing the Ocean placeholder while it waits, so the node has a
    // child even though real data hasn't arrived yet
    CPPUNIT_ASSERT(tile.is_loaded());
}
