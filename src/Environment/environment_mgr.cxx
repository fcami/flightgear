/*
 * SPDX-FileName: environment_mgr.cxx
 * SPDX-FileComment: manager for natural environment information
 * SPDX-FileCopyrightText: 2002 David Megginson <david@megginson.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>

#include <cstring>

#include <simgear/constants.h>
#include <simgear/debug/logstream.hxx>
#include <simgear/timing/sg_time.hxx>

#include <simgear/scene/model/particles.hxx>
#include <simgear/scene/sky/sky.hxx>
#include <simgear/structure/event_mgr.hxx>

#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Main/main.hxx>
#include <Viewer/ViewPropertyEvaluator.hxx>
#include <Viewer/renderer.hxx>

#include <FDM/flight.hxx>

#include "Airports/airport.hxx"
#include "climate.hxx"
#include "environment.hxx"
#include "environment_ctrl.hxx"
#include "environment_mgr.hxx"
#include "fgclouds.hxx"
#include "gravity.hxx"
#include "magvarmanager.hxx"
#include "precipitation_mgr.hxx"
#include "realwx_ctrl.hxx"
#include "ridge_lift.hxx"
#include "terrainsampler.hxx"

#include "AIModel/AINotifications.hxx"

FGEnvironmentMgr::FGEnvironmentMgr() : _environment(new FGEnvironment()),
                                       _multiplayerListener(nullptr),
                                       _sky(globals->get_renderer() ? globals->get_renderer()->getSky() : nullptr),
                                       nearestCarrier(nullptr),
                                       nearestAirport(nullptr)
{
    fgClouds = new FGClouds;
    if (_sky) {
        _sky->getPreRoot()->addChild(fgClouds->getCloudUpdateNode());
    }
    set_subsystem("controller", Environment::LayerInterpolateController::createInstance(fgGetNode("/environment/config", true)));

    set_subsystem("climate", new FGClimate);
    set_subsystem("precipitation", new FGPrecipitationMgr);
    set_subsystem("realwx", Environment::RealWxController::createInstance(fgGetNode("/environment/realwx", true)), 1.0);
    set_subsystem("terrainsampler", Environment::TerrainSampler::createInstance(fgGetNode("/environment/terrain", true)));
    set_subsystem("ridgelift", new FGRidgeLift);

    set_subsystem("magvar", new FGMagVarManager);
    max_tower_height_feet = fgGetDouble("/sim/airport/max-tower-height-ft", 70);
    min_tower_height_feet = fgGetDouble("/sim/airport/min-tower-height-ft", 6);
    default_tower_height_feet = fgGetDouble("default-tower-height-ft", 30);
}

FGEnvironmentMgr::~FGEnvironmentMgr()
{
    remove_subsystem("ridgelift");
    remove_subsystem("terrainsampler");
    remove_subsystem("precipitation");
    remove_subsystem("realwx");
    remove_subsystem("controller");
    remove_subsystem("magvar");

    delete fgClouds;
    delete _environment;
}

struct FGEnvironmentMgrMultiplayerListener : SGPropertyChangeListener {
    FGEnvironmentMgrMultiplayerListener(FGEnvironmentMgr* environmentmgr)
        : _environmentmgr(environmentmgr)
    {
        _node = fgGetNode("/sim/current-view/model-view", true /*create*/);
        _node->addChangeListener(this);
    }
    virtual void valueChanged(SGPropertyNode* node)
    {
        _environmentmgr->updateClosestAirport();
    }
    virtual ~FGEnvironmentMgrMultiplayerListener()
    {
        _node->removeChangeListener(this);
    }

private:
    FGEnvironmentMgr* _environmentmgr;
    SGPropertyNode_ptr _node;
};

SGSubsystem::InitStatus FGEnvironmentMgr::incrementalInit()
{
    InitStatus r = SGSubsystemGroup::incrementalInit();
    if (r == INIT_DONE) {
        fgClouds->Init();
        _multiplayerListener = new FGEnvironmentMgrMultiplayerListener(this);
        globals->get_event_mgr()->addTask("updateClosestAirport", [this]() { this->updateClosestAirport(); }, 10);
    }

    return r;
}

void FGEnvironmentMgr::shutdown()
{
    globals->get_event_mgr()->removeTask("updateClosestAirport");
    delete _multiplayerListener;
    _multiplayerListener = nullptr;
    SGSubsystemGroup::shutdown();
}

void FGEnvironmentMgr::reinit()
{
    SG_LOG(SG_ENVIRONMENT, SG_INFO, "Reinitializing environment subsystem");
    SGSubsystemGroup::reinit();
}

void FGEnvironmentMgr::bind()
{
    SGSubsystemGroup::bind();
    _environment->Tie(fgGetNode("/environment", true));

    _tiedProperties.setRoot(fgGetNode("/environment", true));

    _tiedProperties.Tie("effective-visibility-m", _sky,
                        &SGSky::get_visibility);

    _tiedProperties.Tie("rebuild-layers", fgClouds,
                        &FGClouds::get_update_event,
                        &FGClouds::set_update_event);
    //  _tiedProperties.Tie("turbulence/use-cloud-turbulence", &sgEnviro,
    //          &SGEnviro::get_turbulence_enable_state,
    //          &SGEnviro::set_turbulence_enable_state);

    for (int i = 0; i < MAX_CLOUD_LAYERS; i++) {
        SGPropertyNode_ptr layerNode = fgGetNode("/environment/clouds", true)->getChild("layer", i, true);

        _tiedProperties.Tie(layerNode->getNode("span-m", true), this, i,
                            &FGEnvironmentMgr::get_cloud_layer_span_m,
                            &FGEnvironmentMgr::set_cloud_layer_span_m);

        _tiedProperties.Tie(layerNode->getNode("elevation-ft", true), this, i,
                            &FGEnvironmentMgr::get_cloud_layer_elevation_ft,
                            &FGEnvironmentMgr::set_cloud_layer_elevation_ft);

        _tiedProperties.Tie(layerNode->getNode("thickness-ft", true), this, i,
                            &FGEnvironmentMgr::get_cloud_layer_thickness_ft,
                            &FGEnvironmentMgr::set_cloud_layer_thickness_ft);

        _tiedProperties.Tie(layerNode->getNode("transition-ft", true), this, i,
                            &FGEnvironmentMgr::get_cloud_layer_transition_ft,
                            &FGEnvironmentMgr::set_cloud_layer_transition_ft);

        _tiedProperties.Tie(layerNode->getNode("coverage", true), this, i,
                            &FGEnvironmentMgr::get_cloud_layer_coverage,
                            &FGEnvironmentMgr::set_cloud_layer_coverage);

        _tiedProperties.Tie(layerNode->getNode("coverage-type", true), this, i,
                            &FGEnvironmentMgr::get_cloud_layer_coverage_type,
                            &FGEnvironmentMgr::set_cloud_layer_coverage_type);

        _tiedProperties.Tie(layerNode->getNode("visibility-m", true), this, i,
                            &FGEnvironmentMgr::get_cloud_layer_visibility_m,
                            &FGEnvironmentMgr::set_cloud_layer_visibility_m);

        _tiedProperties.Tie(layerNode->getNode("alpha", true), this, i,
                            &FGEnvironmentMgr::get_cloud_layer_maxalpha,
                            &FGEnvironmentMgr::set_cloud_layer_maxalpha);
    }

    _tiedProperties.setRoot(fgGetNode("/sim/rendering", true));

    _tiedProperties.Tie("clouds3d-vis-range", _sky,
                        &SGSky::get_3dCloudVisRange,
                        &SGSky::set_3dCloudVisRange);
}

void FGEnvironmentMgr::unbind()
{
    _tiedProperties.Untie();
    _environment->Untie();
    SGSubsystemGroup::unbind();
}

void FGEnvironmentMgr::update(double dt)
{
    SGGeod aircraftPos(globals->get_aircraft_position());

    SGSubsystemGroup::update(dt);

    _environment->set_elevation_ft(aircraftPos.getElevationFt());

    auto particlesManager = simgear::ParticlesGlobalManager::instance();
    particlesManager->setWindFrom(_environment->get_wind_from_heading_deg(),
                                  _environment->get_wind_speed_kt());
    particlesManager->update(dt, globals->get_aircraft_position());

    if (rebuildCloudLayers) {
        rebuildCloudLayers = false;
        fgClouds->set_update_event(fgClouds->get_update_event() + 1);
    }

    updateTowerPosition();

    updateEnvironmentCache(dt);

    fgClouds->updateWindColumn(dt, _environment);

    fgSetDouble("/environment/gravitational-acceleration-mps2",
                Environment::Gravity::instance()->getGravity(aircraftPos));
}

void FGEnvironmentMgr::updateEnvironmentCache(double dt)
{
    for (auto& entry : environmentCache) {
        if (entry.fetcher) {
            entry.fetcher->update(dt);
        }
        syncCacheEntryFromNode(*entry.env, entry.node);
    }
}

void FGEnvironmentMgr::updateTowerPosition()
{
    if (towerViewPositionLatDegNode != nullptr && towerViewPositionLonDegNode != nullptr && towerViewPositionAltFtNode != nullptr) {
        auto automaticTowerActive = fgGetBool("/sim/tower/auto-position", true);

        fgSetDouble("/sim/airport/nearest-tower-latitude-deg", towerViewPositionLatDegNode->getDoubleValue());
        fgSetDouble("/sim/airport/nearest-tower-longitude-deg", towerViewPositionLonDegNode->getDoubleValue());
        fgSetDouble("/sim/airport/nearest-tower-altitude-ft", towerViewPositionAltFtNode->getDoubleValue());

        if (automaticTowerActive) {
            fgSetDouble("/sim/tower/latitude-deg", towerViewPositionLatDegNode->getDoubleValue());
            fgSetDouble("/sim/tower/longitude-deg", towerViewPositionLonDegNode->getDoubleValue());
            fgSetDouble("/sim/tower/altitude-ft", towerViewPositionAltFtNode->getDoubleValue());
        }
    }
}

void FGEnvironmentMgr::updateClosestAirport()
{
    SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "FGEnvironmentMgr::update: updating closest airport");

    SGGeod pos = globals->get_aircraft_position();

    //
    // If we are viewing a multiplayer aircraft, find nearest airport so that
    // Tower View etc works.
    std::string view_config_root = ViewPropertyEvaluator::getStringValue("(/sim/view[(/sim/current-view/view-number-raw)]/config/root)");

    if (view_config_root != "/" && view_config_root != "") {
        /* We are currently viewing a multiplayer aircraft. */
        pos = SGGeod::fromDegFt(
            ViewPropertyEvaluator::getDoubleValue("((/sim/view[(/sim/current-view/view-number-raw)]/config/root)/position/longitude-deg)"),
            ViewPropertyEvaluator::getDoubleValue("((/sim/view[(/sim/current-view/view-number-raw)]/config/root)/position/latitude-deg)"),
            ViewPropertyEvaluator::getDoubleValue("((/sim/view[(/sim/current-view/view-number-raw)]/config/root)/position/altitude-ft)"));
    }

    // nearest tower logic;
    // 1. find nearest airport
    // 2. find nearest carrier
    // - select the nearest one as the tower.

    nearestAirport = FGAirport::findClosest(pos, 100.0);
    auto automaticTowerActive = fgGetBool("/sim/tower/auto-position", true);

    SGGeod nearestTowerPosition;
    std::string nearestIdent;
    const SGGeod airportGeod;
    double towerDistance = std::numeric_limits<double>::max();
    if (nearestAirport) {
        const std::string currentId = fgGetString("/sim/airport/closest-airport-id", "");
        if (currentId != nearestAirport->ident()) {
            SG_LOG(SG_ENVIRONMENT, SG_INFO, "FGEnvironmentMgr::updateClosestAirport: selected:" << nearestAirport->ident());
            fgSetString("/sim/airport/closest-airport-id", nearestAirport->ident().c_str());
        }

        if (nearestAirport->hasTower()) {
            nearestTowerPosition = nearestAirport->getTowerLocation();
            SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "airport-id=" << nearestAirport->getId() << " tower_pos=" << nearestTowerPosition);
        } else {
            nearestTowerPosition = nearestAirport->geod();
            SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "no tower for airport-id=" << nearestAirport->getId());
        }
        //Ensure that the tower isn't at ground level by adding a nominal amount
        // TODO: (fix the data so that too short or too tall towers aren't present in the data)
        auto towerAirpotDistance = abs(nearestTowerPosition.getElevationFt() - nearestAirport->geod().getElevationFt());
        if (towerAirpotDistance < min_tower_height_feet) {
            nearestTowerPosition.setElevationFt(nearestTowerPosition.getElevationFt() + default_tower_height_feet);
            SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Tower altitude adjusted because it was at below minimum height above ground (" << min_tower_height_feet << "feet) for airport " << nearestAirport->getId());
        } else if (towerAirpotDistance > max_tower_height_feet) {
            nearestTowerPosition.setElevationFt(nearestTowerPosition.getElevationFt() + default_tower_height_feet);
            SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Tower altitude adjusted because it was taller than the permitted maximum of (" << max_tower_height_feet << "feet) for airport " << nearestAirport->getId());
        }
        //
        nearestIdent = nearestAirport->ident();
        towerDistance = SGGeodesy::distanceM(nearestTowerPosition, pos);

        // when the tower doesn't move we can clear these.
        // if the carrier is nearer these variables will be set in that logic.
        towerViewPositionLatDegNode = towerViewPositionLonDegNode = towerViewPositionAltFtNode = nullptr;
    } else {
        SG_LOG(SG_ENVIRONMENT, SG_INFO, "FGEnvironmentMgr::update: No airport within 100NM range");
    }
    auto nctn = SGSharedPtr<NearestCarrierToNotification>(new NearestCarrierToNotification(pos));
    if (simgear::Emesary::ReceiptStatus::OK == simgear::Emesary::GlobalTransmitter::instance()->NotifyAll(nctn)) {
        if (nearestCarrier != nctn->GetCarrier()) {
            nearestCarrier = nctn->GetCarrier();
            fgSetString("/sim/airport/nearest-carrier", nctn->GetCarrierIdent());
        }
    } else {
        fgSetString("/sim/airport/nearest-carrier", "");
        fgSetDouble("/sim/airport/nearest-carrier-latitude-deg", 0);
        fgSetDouble("/sim/airport/nearest-carrier-longitude-deg", 0);
        fgSetDouble("/sim/airport/nearest-carrier-altitude-ft", 0);
        fgSetDouble("/sim/airport/nearest-carrier-deck-height", 0);
        nearestCarrier = nullptr;
    }

    // figure out if the carrier's tower is closer
    if (nearestCarrier && (nctn->GetDistanceMeters() < towerDistance)) {
        nearestIdent = nctn->GetCarrierIdent();

        //
        // these will be used to determine and update the tower position
        towerViewPositionLatDegNode = nctn->GetViewPositionLatNode();
        towerViewPositionLonDegNode = nctn->GetViewPositionLonNode();
        towerViewPositionAltFtNode = nctn->GetViewPositionAltNode();

        // although the carrier is moving - these values can afford to be 10 seconds old so we don't need to
        // update them.
        fgSetDouble("/sim/airport/nearest-carrier-latitude-deg", nctn->GetPosition()->getLatitudeDeg());
        fgSetDouble("/sim/airport/nearest-carrier-longitude-deg", nctn->GetPosition()->getLongitudeDeg());
        fgSetDouble("/sim/airport/nearest-carrier-altitude-ft", nctn->GetPosition()->getElevationFt());
        fgSetDouble("/sim/airport/nearest-carrier-deck-height", nctn->GetDeckheight());
    } else {
        if (nearestAirport != nullptr) {
            if (automaticTowerActive) {
                std::string path = ViewPropertyEvaluator::getStringValue("(/sim/view[(/sim/current-view/view-number-raw)]/config/root)/sim/tower/");
                fgSetString(path + "airport-id", nearestAirport->getId());

                fgSetDouble(path + "latitude-deg", nearestTowerPosition.getLatitudeDeg());
                fgSetDouble(path + "longitude-deg", nearestTowerPosition.getLongitudeDeg());
                fgSetDouble(path + "altitude-ft", nearestTowerPosition.getElevationFt());
            }
        } else {
            SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "FGEnvironmentMgr::update: No airport or carrier within 100NM range of current multiplayer aircraft");
        }
    }


    if (fgGetString("/sim/airport/nearest-tower-ident") != nearestIdent) {
        SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Nearest airport tower now " << nearestIdent);
        fgSetString("/sim/airport/nearest-tower-ident", nearestIdent);
    }
    if (automaticTowerActive) {
        if (fgGetString("/sim/tower/airport-id") != nearestIdent) {
            fgSetString("/sim/tower/airport-id", nearestIdent);
            SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Auto Tower: now " << nearestIdent);
        }
    }
    updateTowerPosition();
}


FGEnvironment
FGEnvironmentMgr::getEnvironment() const
{
    return *_environment;
}

const FGEnvironment* FGEnvironmentMgr::getAircraftEnvironment() const
{
    return _environment;
}

void FGEnvironmentMgr::syncCacheEntryFromNode(FGEnvironment& env, const SGPropertyNode* node) const
{
    // Raw METAR data lives under a "metar-data" child subtree, entirely
    // separate from this FGEnvironment's own tied properties on `node`.
    // This is deliberate: simgear::MetarProperties/LiveMetarProperties (see
    // metarproperties.cxx) ties its own ~30 leaf names for its raw parsed
    // fields, several of which (temperature-degc, dewpoint-degc,
    // pressure-inhg, ...) are also FGEnvironment property names
    const SGPropertyNode* metarData = node ? node->getChild("metar-data") : nullptr;
    if (!metarData || !metarData->getBoolValue("valid", false)) {
        env.setValid(false);
        return;
    }

    // The three primary readings; FGEnvironment derives the sea-level
    // variants, density, relative humidity etc. from these automatically
    // (see set_temperature_degc/set_dewpoint_degc/set_pressure_inhg).
    env.set_temperature_degc(metarData->getDoubleValue("temperature-degc", env.get_temperature_degc()));
    env.set_dewpoint_degc(metarData->getDoubleValue("dewpoint-degc", env.get_dewpoint_degc()));
    env.set_pressure_inhg(metarData->getDoubleValue("pressure-inhg", env.get_pressure_inhg()));

    // MetarProperties uses "base-wind-*"/"*-visibility-m" names (matching
    // the legacy single /environment/metar node), not FGEnvironment's own
    // "wind-*"/"visibility-m" - map them across explicitly.
    env.set_wind_speed_kt(metarData->getDoubleValue("base-wind-speed-kt", env.get_wind_speed_kt()));
    env.set_wind_from_heading_deg(metarData->getDoubleValue("base-wind-dir-deg", env.get_wind_from_heading_deg()));
    const double minVisibilityM = metarData->getDoubleValue("min-visibility-m", -1.0);
    if (minVisibilityM >= 0.0) {
        env.set_visibility_m(minVisibilityM);
    }

    env.setValid(true);
    env.setLastUpdateTime(globals->get_sim_time_sec());
}

double FGEnvironmentMgr::cacheMatchRadiusM(const SGGeod& aPos) const
{
    // Use a tight cache radius for positions near the user (so nearby
    // airports/AI traffic get distinct, locally-accurate weather), and a
    // much coarser one for distant positions (so the rest of the world
    // doesn't need a separate cache entry per airport). Blended linearly by
    // great-circle distance from the user's own position: 0nm away uses
    // the near radius, 2000nm away uses the far value
    const double nearRadiusNm = fgGetDouble("/sim/environment/metar/cache-distance-near-nm", 50.0);
    const double farRadiusNm = fgGetDouble("/sim/environment/metar/cache-distance-far-nm", 640.0);
    const double farCutoff = 2000.0;

    const double d = SGGeodesy::distanceM(globals->get_aircraft_position(), aPos) * SG_METER_TO_NM;
    const double t = SGMiscd::clip(d / farCutoff, 0.0, 1.0);
    const double radiusNm = nearRadiusNm + t * (farRadiusNm - nearRadiusNm);
    return radiusNm * SG_NM_TO_METER;
}

FGEnvironment
FGEnvironmentMgr::getEnvironmentAtPosition(const SGGeod& aPos)
{
    const int cacheSize = fgGetInt("/sim/environment/metar/cache-size", 128);
    const double cacheDistanceM = cacheMatchRadiusM(aPos);
    // How long an entry may go un-requested before it's considered stale and
    // repurposed for a new position, even though the cache isn't full yet.
    // Kept constant regardless of distance from the user, unlike the
    // match radius above - staleness is about "has the weather likely
    // changed", not about how precisely we're tracking the position.
    const double cacheAgeSeconds = fgGetDouble("/sim/environment/metar/cache-age-seconds", 3600.0);
    const double now = globals->get_sim_time_sec();

    auto old = [](const CachedEnvironment& a, const CachedEnvironment& b) {
        if (!a.env || !b.env) {
            SG_LOG(SG_ENVIRONMENT, SG_DEV_ALERT, "Encountered a null cached environment during eviction comparison");
            return false;
        }
        return a.env->getLastRequestTime() < b.env->getLastRequestTime();
    };

    auto distanceSqrTo = [&](const CachedEnvironment& e) {
        return distSqr(SGVec3d::fromGeod(e.env->geod()), SGVec3d::fromGeod(aPos));
    };

    auto nearestAirport = FGAirport::findClosest(aPos, 100.0);
    const SGGeod snappedPos = nearestAirport ? nearestAirport->geod() : aPos;

    auto closest = std::min_element(environmentCache.begin(), environmentCache.end(),
                                    [&](const CachedEnvironment& a, const CachedEnvironment& b) {
                                        return distanceSqrTo(a) < distanceSqrTo(b);
                                    });
    const bool haveClose = closest != environmentCache.end() && distanceSqrTo(*closest) < cacheDistanceM * cacheDistanceM;
    const bool isStale = haveClose && (now - closest->env->getLastRequestTime()) > cacheAgeSeconds;

    if (haveClose && !isStale) {
        // cache hit; return the cached environment
        SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "METAR Cache hit for position " << aPos);
        closest->env->setLastRequestTime(now);
        return *closest->env;
    }

    if (haveClose && isStale) {
        // Found an entry covering this position, but nobody has asked for it in
        // a while: repurpose it in place (same cache slot, same property node,
        // same fetcher - just re-target it) rather than growing the cache or
        // evicting a different entry.
        SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Refreshing stale cached environment for position " << aPos);
        closest->env->setGeod(snappedPos);
        closest->env->setValid(false);
        closest->env->setLastRequestTime(now);
        requestMetarFor(*closest, nearestAirport);
        return *closest->env;
    }

    if (environmentCache.size() >= static_cast<size_t>(cacheSize)) {
        SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Environment cache is full, dropping oldest.");
        auto oldest = std::min_element(environmentCache.begin(), environmentCache.end(), old);
        if (oldest != environmentCache.end()) {
            SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Evicting environment for position " << oldest->env->geod() << " last requested at " << oldest->env->getLastRequestTime());
            oldest->env->setGeod(snappedPos);
            oldest->env->setValid(false);
            oldest->env->setLastRequestTime(now);
            requestMetarFor(*oldest, nearestAirport);
            return *oldest->env;
        }
        SG_LOG(SG_ENVIRONMENT, SG_DEV_ALERT, "Environment cache is full but no environment to evict found. This should not happen.");
        FGEnvironment env = *_environment;
        env.set_elevation_ft(aPos.getElevationFt());
        return env;
    }

    // cache miss; create a new environment for this position
    auto env = std::make_unique<FGEnvironment>(*_environment);
    SGPropertyNode_ptr node = fgGetNode("/environment", true)->addChild("metar");
    // Tie() first: it can push the (still-default) property values back
    // through the setters, so it must run before we set this entry's real
    // position/state below, not after.
    env->Tie(node);
    env->set_elevation_ft(aPos.getElevationFt());
    env->setGeod(snappedPos);
    env->setValid(false);
    env->setLastRequestTime(now);

    const FGEnvironment result = *env;
    environmentCache.push_back(CachedEnvironment{std::move(env), node, nullptr});
    requestMetarFor(environmentCache.back(), nearestAirport);
    SG_LOG(SG_ENVIRONMENT, SG_BULK, "Created new environment for position " << aPos << " Cache Size: " << environmentCache.size());
    return result;
}

Environment::MetarRequester* FGEnvironmentMgr::getMetarRequester()
{
    auto* realWx = dynamic_cast<Environment::RealWxController*>(get_subsystem("realwx"));
    return realWx ? realWx->getMetarRequester() : nullptr;
}

void FGEnvironmentMgr::requestMetarFor(CachedEnvironment& entry, const FGAirport* nearestAirport)
{
    const std::string stationId = nearestAirport ? nearestAirport->ident() : std::string();

    if (!entry.fetcher) {
        auto* requester = getMetarRequester();
        if (!requester) {
            // No real-weather source configured (e.g. offline/canned weather);
            // the cache entry stays at its snapshot-of-_environment defaults.
            return;
        }
        // Raw METAR fields go under a "metar-data" child, not `entry.node`
        // itself - see syncCacheEntryFromNode() for why.
        SGPropertyNode_ptr metarDataNode = entry.node->getChild("metar-data", 0, true);
        // A coarser refresh interval than LiveMetarProperties' own default
        // (900s, tuned for the single primary/aircraft station): with up to
        // cache-size entries potentially live at once, refreshing every one
        // of them every 15 minutes is a lot of unnecessary fetching for
        // airports nobody is closely watching.
        const double refreshIntervalSeconds = fgGetDouble("/sim/environment/metar/cache-refresh-seconds", 1800.0);
        entry.fetcher = new Environment::LiveMetarProperties(metarDataNode, requester, /*maxAge*/ 0, refreshIntervalSeconds);
    }

    entry.fetcher->setStationId(stationId);
    entry.fetcher->resetTimeToLive(); // force an immediate (re-)fetch
}

double
FGEnvironmentMgr::get_cloud_layer_span_m(int index) const
{
    return _sky->get_cloud_layer(index)->getSpan_m();
}

void FGEnvironmentMgr::set_cloud_layer_span_m(int index, double span_m)
{
    _sky->get_cloud_layer(index)->setSpan_m(span_m);
}

double
FGEnvironmentMgr::get_cloud_layer_elevation_ft(int index) const
{
    return _sky->get_cloud_layer(index)->getElevation_m() * SG_METER_TO_FEET;
}

void FGEnvironmentMgr::set_cloud_layer_elevation_ft(int index, double elevation_ft)
{
    FGEnvironment env = *_environment;
    env.set_elevation_ft(elevation_ft);

    _sky->get_cloud_layer(index)
        ->setElevation_m(elevation_ft * SG_FEET_TO_METER);

    _sky->get_cloud_layer(index)
        ->setSpeed(env.get_wind_speed_kt() * 0.5151); // 1 kt = 0.5151 m/s

    _sky->get_cloud_layer(index)
        ->setDirection(env.get_wind_from_heading_deg());
}

double
FGEnvironmentMgr::get_cloud_layer_thickness_ft(int index) const
{
    return _sky->get_cloud_layer(index)->getThickness_m() * SG_METER_TO_FEET;
}

void FGEnvironmentMgr::set_cloud_layer_thickness_ft(int index, double thickness_ft)
{
    _sky->get_cloud_layer(index)
        ->setThickness_m(thickness_ft * SG_FEET_TO_METER);
}

double
FGEnvironmentMgr::get_cloud_layer_transition_ft(int index) const
{
    return _sky->get_cloud_layer(index)->getTransition_m() * SG_METER_TO_FEET;
}

void FGEnvironmentMgr::set_cloud_layer_transition_ft(int index,
                                                     double transition_ft)
{
    _sky->get_cloud_layer(index)
        ->setTransition_m(transition_ft * SG_FEET_TO_METER);
}

const char*
FGEnvironmentMgr::get_cloud_layer_coverage(int index) const
{
    return _sky->get_cloud_layer(index)->getCoverageString().c_str();
}

void FGEnvironmentMgr::set_cloud_layer_coverage(int index,
                                                const char* coverage_name)
{
    if (_sky->get_cloud_layer(index)->getCoverageString() == coverage_name)
        return;

    _sky->get_cloud_layer(index)->setCoverageString(coverage_name);
    rebuildCloudLayers = true;
}

int FGEnvironmentMgr::get_cloud_layer_coverage_type(int index) const
{
    return _sky->get_cloud_layer(index)->getCoverage();
}

double
FGEnvironmentMgr::get_cloud_layer_visibility_m(int index) const
{
    return _sky->get_cloud_layer(index)->getVisibility_m();
}

void FGEnvironmentMgr::set_cloud_layer_visibility_m(int index, double visibility_m)
{
    _sky->get_cloud_layer(index)->setVisibility_m(visibility_m);
}

double
FGEnvironmentMgr::get_cloud_layer_maxalpha(int index) const
{
    return _sky->get_cloud_layer(index)->getMaxAlpha();
}

void FGEnvironmentMgr::set_cloud_layer_maxalpha(int index, double maxalpha)
{
    _sky->get_cloud_layer(index)->setMaxAlpha(maxalpha);
}

void FGEnvironmentMgr::set_cloud_layer_coverage_type(int index, int type)
{
    if (type < 0 || type >= SGCloudLayer::SG_MAX_CLOUD_COVERAGES) {
        SG_LOG(SG_ENVIRONMENT, SG_WARN, "Unknown cloud layer type " << type << " ignored");
        return;
    }

    if (static_cast<SGCloudLayer::Coverage>(type) == _sky->get_cloud_layer(index)->getCoverage())
        return;

    _sky->get_cloud_layer(index)->setCoverage(static_cast<SGCloudLayer::Coverage>(type));
    rebuildCloudLayers = true;
}


// Register the subsystem.
SGSubsystemMgr::Registrant<FGEnvironmentMgr> registrantFGEnvironmentMgr;

// end of environment-mgr.cxx
