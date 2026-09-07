// test_environment_cache.cxx - unit tests for FGEnvironmentMgr's per-position
// METAR/environment cache.
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 James Turner

#include "config.h"

#include "test_environment_cache.hxx"

#include <cmath>

#include "test_suite/FGTestApi/NavDataCache.hxx"
#include "test_suite/FGTestApi/testGlobals.hxx"

#include <Airports/airport.hxx>
#include <Environment/environment.hxx>
#include <Environment/environment_mgr.hxx>
#include <Environment/realwx_ctrl.hxx>
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>

#include <simgear/math/sg_geodesy.hxx>

// A MetarRequester that answers synchronously with a canned METAR string (or
// simulates a failure, if none has been set), instead of making a real HTTP
// request - see Environment::MetarRequester in realwx_ctrl.hxx.
class FakeMetarRequester : public Environment::MetarRequester
{
public:
    void requestMetar(Environment::LiveMetarProperties_ptr handler, const std::string& id) override
    {
        ++requestCount;
        lastStationId = id;
        if (canned.empty()) {
            handler->handleMetarFailure();
        } else {
            handler->handleMetarData(canned);
        }
    }

    std::string canned;
    std::string lastStationId;
    int requestCount = 0;
};

namespace {

class FakeRealWxController : public Environment::RealWxController
{
public:
    FakeRealWxController() : _requester(new FakeMetarRequester) {}
    ~FakeRealWxController() override { delete _requester; }

    void update(double) override {}
    Environment::MetarRequester* getMetarRequester() override { return _requester; }

    FakeMetarRequester* fake() const { return _requester; }

private:
    FakeMetarRequester* _requester;
};

} // namespace

void EnvironmentCacheTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("environment-cache");
    FGTestApi::setUp::initNavDataCache();

    // Small, deterministic cache config for the tests below. Near/far match
    // radii use the production defaults (see cacheMatchRadiusM()) since most
    // tests only care about exact-position hits; testCacheRadiusVariesWithPlayerDistance()
    // exercises the near/far blending itself.
    fgSetInt("/sim/environment/metar/cache-size", 2);
    fgSetDouble("/sim/environment/metar/cache-distance-near-nm", 50.0);
    fgSetDouble("/sim/environment/metar/cache-distance-far-nm", 640.0);
    fgSetDouble("/sim/environment/metar/cache-age-seconds", 60.0);

    // cacheMatchRadiusM() scales with distance from the player's own
    // position, so most tests want a stable reference point - default it to
    // EDDF, which is what most of them query.
    auto eddf = FGAirport::getByIdent("EDDF");
    fgSetDouble("/position/latitude-deg", eddf->geod().getLatitudeDeg());
    fgSetDouble("/position/longitude-deg", eddf->geod().getLongitudeDeg());
    fgSetDouble("/position/altitude-ft", eddf->geod().getElevationFt());

    // Register FGEnvironmentMgr with globals' subsystem manager (some code
    // reachable from METAR parsing, e.g. MetarProperties::setMetar(), looks
    // it up via globals->get_subsystem<FGEnvironmentMgr>()) but never call
    // bind()/init() on the manager: bind() ties cloud-layer properties
    // through a real SGSky (unavailable/not fully populated headless) and
    // several child subsystems (FGClimate, FGPrecipitationMgr, ...) require
    // init()/bind() to have run before update() is safe to call. None of
    // that is needed to exercise the position cache itself, so drop every
    // child but the METAR fetcher we actually want (a fake one, so tests
    // don't hit the network).
    auto envMgr = globals->get_subsystem_mgr()->add<FGEnvironmentMgr>();
    envMgr->clearSubsystems();
    auto* fakeRealWx = new FakeRealWxController();
    envMgr->set_subsystem("realwx", fakeRealWx);
    _requester = fakeRealWx->fake();
    _envMgr = envMgr.get();
}

void EnvironmentCacheTests::tearDown()
{
    // Owned by globals' subsystem manager; shutdownTestGlobals() below
    // destroys globals (and with it, the subsystem manager and _envMgr).
    _envMgr = nullptr;
    _requester = nullptr;
    FGTestApi::tearDown::shutdownTestGlobals();
}

void EnvironmentCacheTests::testCacheCreatesDistinctEntries()
{
    auto eddf = FGAirport::getByIdent("EDDF");
    auto ksfo = FGAirport::getByIdent("KSFO");

    FGEnvironment e1 = _envMgr->getEnvironmentAtPosition(eddf->geod());
    FGEnvironment e2 = _envMgr->getEnvironmentAtPosition(ksfo->geod());

    CPPUNIT_ASSERT(SGGeodesy::distanceM(e1.geod(), eddf->geod()) < 100.0);
    CPPUNIT_ASSERT(SGGeodesy::distanceM(e2.geod(), ksfo->geod()) < 100.0);
    CPPUNIT_ASSERT(SGGeodesy::distanceM(e1.geod(), e2.geod()) > 100000.0);

    // Two distinct positions, two fetches - each getEnvironmentAtPosition()
    // call only arms the corresponding fetcher (resetTimeToLive()); the
    // actual MetarRequester::requestMetar() call happens on the next
    // update() tick.
    _envMgr->updateEnvironmentCache(0.1);
    CPPUNIT_ASSERT_EQUAL(2, _requester->requestCount);
}

void EnvironmentCacheTests::testCacheReusesNearbyEntry()
{
    auto eddf = FGAirport::getByIdent("EDDF");

    _envMgr->getEnvironmentAtPosition(eddf->geod());
    _envMgr->updateEnvironmentCache(0.1);
    const int countAfterFirst = _requester->requestCount;
    CPPUNIT_ASSERT_EQUAL(1, countAfterFirst);

    // Same position again: should be a cache hit, no new fetch triggered.
    FGEnvironment again = _envMgr->getEnvironmentAtPosition(eddf->geod());
    _envMgr->updateEnvironmentCache(0.1);
    CPPUNIT_ASSERT_EQUAL(countAfterFirst, _requester->requestCount);
    CPPUNIT_ASSERT(SGGeodesy::distanceM(again.geod(), eddf->geod()) < 100.0);
}

void EnvironmentCacheTests::testCacheLRUEviction()
{
    // cache-size is 2 (see setUp).
    auto eddf = FGAirport::getByIdent("EDDF");
    auto ksfo = FGAirport::getByIdent("KSFO");
    auto lowi = FGAirport::getByIdent("LOWI");

    _envMgr->getEnvironmentAtPosition(eddf->geod()); // fills slot 1
    _envMgr->getEnvironmentAtPosition(ksfo->geod()); // fills slot 2 (cache now full)
    _envMgr->updateEnvironmentCache(0.1);
    CPPUNIT_ASSERT_EQUAL(2, _requester->requestCount);

    // A third, distinct position forces an eviction (EDDF was requested
    // first, so it's the least-recently-requested entry).
    _envMgr->getEnvironmentAtPosition(lowi->geod());
    _envMgr->updateEnvironmentCache(0.1);
    CPPUNIT_ASSERT_EQUAL(3, _requester->requestCount);

    // Re-querying EDDF now must miss (its slot was repurposed for LOWI) and
    // trigger a fresh fetch again.
    _envMgr->getEnvironmentAtPosition(eddf->geod());
    _envMgr->updateEnvironmentCache(0.1);
    CPPUNIT_ASSERT_EQUAL(4, _requester->requestCount);
}

void EnvironmentCacheTests::testCacheAgeRefresh()
{
    auto eddf = FGAirport::getByIdent("EDDF");

    _envMgr->getEnvironmentAtPosition(eddf->geod());
    _envMgr->updateEnvironmentCache(0.1);
    CPPUNIT_ASSERT_EQUAL(1, _requester->requestCount);

    // Still fresh: a repeat query should not re-fetch.
    _envMgr->getEnvironmentAtPosition(eddf->geod());
    _envMgr->updateEnvironmentCache(0.1);
    CPPUNIT_ASSERT_EQUAL(1, _requester->requestCount);

    // Advance sim time well past cache-age-seconds (60s, see setUp).
    globals->inc_sim_time_sec(120.0);

    // Same position, but now stale: refreshed in place, triggering a new fetch.
    _envMgr->getEnvironmentAtPosition(eddf->geod());
    _envMgr->updateEnvironmentCache(0.1);
    CPPUNIT_ASSERT_EQUAL(2, _requester->requestCount);
}

void EnvironmentCacheTests::testIsValidReadiness()
{
    auto eddf = FGAirport::getByIdent("EDDF");

    FGEnvironment env = _envMgr->getEnvironmentAtPosition(eddf->geod());
    CPPUNIT_ASSERT_MESSAGE("no fetch has completed yet", !env.isValid());

    _requester->canned = "EDDF 271320Z 08007KT 030V130 CAVOK 17/02 Q1020 NOSIG";
    _envMgr->updateEnvironmentCache(0.1); // ticks the fetcher, which delivers the canned METAR synchronously

    env = _envMgr->getEnvironmentAtPosition(eddf->geod());
    CPPUNIT_ASSERT_MESSAGE("fetch has now completed", env.isValid());
}

void EnvironmentCacheTests::testMetarFieldsNoTieCollision()
{
    auto eddf = FGAirport::getByIdent("EDDF");

    // Before the fetch completes, the entry still reflects the ISA-derived
    // snapshot taken from the aircraft's own environment at creation time
    // (not the METAR's 17/02) - just enough to prove the assertions below
    // are checking a real change, not a coincidence.
    FGEnvironment before = _envMgr->getEnvironmentAtPosition(eddf->geod());
    CPPUNIT_ASSERT(std::abs(before.get_temperature_degc() - 17.0) > 0.5);
    CPPUNIT_ASSERT(std::abs(before.get_dewpoint_degc() - 2.0) > 0.5);

    _requester->canned = "EDDF 271320Z 08007KT 030V130 CAVOK 17/02 Q1020 NOSIG";
    _envMgr->updateEnvironmentCache(0.1);

    FGEnvironment after = _envMgr->getEnvironmentAtPosition(eddf->geod());
    CPPUNIT_ASSERT_MESSAGE("METAR has been applied", after.isValid());

    // If FGEnvironment and MetarProperties were still fighting over the same
    // tied leaf (see syncCacheEntryFromNode()'s comment), these would still
    // read back the pre-fetch defaults above instead of the METAR's values.
    CPPUNIT_ASSERT_DOUBLES_EQUAL(17.0, after.get_temperature_degc(), 0.1);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, after.get_dewpoint_degc(), 0.2);
    // get_pressure_inhg() is station-level (elevation-adjusted from the
    // METAR's QNH), not the raw Q1020 value, so just check it moved off the
    // pre-fetch snapshot rather than asserting an absolute expected value.
    CPPUNIT_ASSERT(std::abs(after.get_pressure_inhg() - before.get_pressure_inhg()) > 0.05);

    // Wind/visibility use different leaf names on each side (MetarProperties'
    // "base-wind-*"/"*-visibility-m" vs FGEnvironment's "wind-*"/"visibility-m"),
    // so they never collided - but for the same reason, they were never
    // bridged either until syncCacheEntryFromNode() mapped them explicitly.
    // "08007KT" = 080 degrees at 7 knots.
    CPPUNIT_ASSERT_DOUBLES_EQUAL(80.0, after.get_wind_from_heading_deg(), 0.5);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(7.0, after.get_wind_speed_kt(), 0.5);
}

void EnvironmentCacheTests::testCacheRadiusVariesWithPlayerDistance()
{
    auto eddf = FGAirport::getByIdent("EDDF"); // Frankfurt
    auto lowi = FGAirport::getByIdent("LOWI"); // Innsbruck, ~215nm from EDDF
    auto ksfo = FGAirport::getByIdent("KSFO"); // San Francisco, ~5300nm from both

    // Player far from both EDDF and LOWI (at KSFO): both fall deep into the
    // "far" (coarse) end of the blend, so their cache-match radius easily
    // covers the ~215nm between them - querying both should share one entry.
    fgSetDouble("/position/latitude-deg", ksfo->geod().getLatitudeDeg());
    fgSetDouble("/position/longitude-deg", ksfo->geod().getLongitudeDeg());
    fgSetDouble("/position/altitude-ft", ksfo->geod().getElevationFt());

    _envMgr->getEnvironmentAtPosition(eddf->geod());
    _envMgr->getEnvironmentAtPosition(lowi->geod());
    _envMgr->updateEnvironmentCache(0.1);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("far from the player, EDDF and LOWI share one coarse entry",
                                 1, _requester->requestCount);

    // Move the player to EDDF: LOWI is now close to the player, well into
    // the "near" (fine) end of the blend, so the same ~215nm gap is now
    // enough to keep them as distinct entries - one more fetch, for LOWI.
    fgSetDouble("/position/latitude-deg", eddf->geod().getLatitudeDeg());
    fgSetDouble("/position/longitude-deg", eddf->geod().getLongitudeDeg());
    fgSetDouble("/position/altitude-ft", eddf->geod().getElevationFt());

    _envMgr->getEnvironmentAtPosition(eddf->geod());
    _envMgr->getEnvironmentAtPosition(lowi->geod());
    _envMgr->updateEnvironmentCache(0.1);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("near the player, EDDF and LOWI get distinct entries",
                                 2, _requester->requestCount);
}
