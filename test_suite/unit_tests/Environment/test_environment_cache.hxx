// test_environment_cache.hxx - unit tests for FGEnvironmentMgr's per-position
// METAR/environment cache (see src/Environment/environment_mgr.hxx).
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 James Turner

#pragma once

#include <memory>

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <Environment/environment_mgr.hxx>

class FakeMetarRequester;

// The unit tests for FGEnvironmentMgr::getEnvironmentAtPosition() and its
// interaction with the METAR fetch pipeline (see realwx_ctrl.hxx).
class EnvironmentCacheTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(EnvironmentCacheTests);
    CPPUNIT_TEST(testCacheCreatesDistinctEntries);
    CPPUNIT_TEST(testCacheReusesNearbyEntry);
    CPPUNIT_TEST(testCacheLRUEviction);
    CPPUNIT_TEST(testCacheAgeRefresh);
    CPPUNIT_TEST(testIsValidReadiness);
    CPPUNIT_TEST(testMetarFieldsNoTieCollision);
    CPPUNIT_TEST(testCacheRadiusVariesWithPlayerDistance);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    // Distinct, far-apart positions get distinct cache entries.
    void testCacheCreatesDistinctEntries();

    // Re-querying (close to) the same position is a cache hit: no new fetch.
    void testCacheReusesNearbyEntry();

    // Once the cache is full, the least-recently-requested entry is
    // repurposed (and re-fetched) for a new position.
    void testCacheLRUEviction();

    // An entry nobody has asked for in a while is refreshed in place, even
    // when the cache isn't full.
    void testCacheAgeRefresh();

    // A freshly-created entry is not valid until the (fake) fetch completes.
    void testIsValidReadiness();

    // Regression test for the FGEnvironment/MetarProperties property-tie
    // collision: temperature/dewpoint/pressure must reach the cached
    // FGEnvironment even though FGEnvironment deliberately does not tie
    // those leaf names on cache-owned nodes (see
    // FGEnvironmentMgr::syncCacheEntryFromNode()).
    void testMetarFieldsNoTieCollision();

    // The cache-match radius scales with distance from the player's own
    // position (globals->get_aircraft_position()): tight nearby, coarse far
    // away - see FGEnvironmentMgr::cacheMatchRadiusM().
    void testCacheRadiusVariesWithPlayerDistance();

private:
    // Owned by globals' subsystem manager (see setUp()) - MetarProperties::setMetar()
    // reaches for globals->get_subsystem<FGEnvironmentMgr>(), so this needs to
    // be registered there for the fetch-completion tests to work, not just
    // constructed standalone.
    FGEnvironmentMgr* _envMgr = nullptr;
    FakeMetarRequester* _requester = nullptr; // owned by _envMgr's "realwx" child
};
