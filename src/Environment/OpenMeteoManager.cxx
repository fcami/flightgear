/*
 * SPDX-FileCopyrightText: 2026 The FlightGear Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "OpenMeteoManager.hxx"

#include "AtmosphericProfile.hxx"
#include "OpenMeteoData.hxx"
#include "WeatherPropertyWriter.hxx"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iterator>
#include <utility>

#include <simgear/debug/logstream.hxx>
#include <simgear/io/HTTPRequest.hxx>
#include <simgear/io/iostreams/sgstream.hxx>
#include <simgear/math/SGMath.hxx>
#include <simgear/timing/sg_time.hxx>

#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Network/HTTPClient.hxx>

namespace {

constexpr double MIN_FETCH_INTERVAL_SEC = 1.0;
constexpr double MIN_FETCH_DISTANCE_M = 1.0;
constexpr double MIN_RETRY_INTERVAL_SEC = 1.0;

constexpr double DEFAULT_LIGHTNING_CONVECTION_THRESHOLD = 0.55;
constexpr double DEFAULT_ICING_THRESHOLD = 0.25;

} // namespace

namespace Environment {

OpenMeteoManager::OpenMeteoManager(SGPropertyNode_ptr rootNode) :
    _rootNode(rootNode)
{
}

OpenMeteoManager::~OpenMeteoManager() = default;

void OpenMeteoManager::init()
{
    if (!_rootNode.valid()) {
        _rootNode = fgGetNode("/environment/open-meteo", true);
    }

    _enabled = _rootNode->getBoolValue("enabled", true);
    _rootNode->setBoolValue("enabled", _enabled);

    _refreshIntervalSec = std::max(MIN_FETCH_INTERVAL_SEC,
                                   _rootNode->getDoubleValue("refresh-seconds", 900.0));
    _refreshDistanceM = std::max(MIN_FETCH_DISTANCE_M,
                                 _rootNode->getDoubleValue("refresh-distance-m", 1000.0));
    _retryIntervalSec = std::max(MIN_RETRY_INTERVAL_SEC,
                                 _rootNode->getDoubleValue("retry-seconds", 60.0));

    _rootNode->setDoubleValue("refresh-seconds", _refreshIntervalSec);
    _rootNode->setDoubleValue("refresh-distance-m", _refreshDistanceM);
    _rootNode->setDoubleValue("retry-seconds", _retryIntervalSec);
    _rootNode->setDoubleValue("lightning/convection-threshold",
                              _rootNode->getDoubleValue("lightning/convection-threshold",
                                                        DEFAULT_LIGHTNING_CONVECTION_THRESHOLD));
    _rootNode->setDoubleValue("icing-threshold",
                              _rootNode->getDoubleValue("icing-threshold", DEFAULT_ICING_THRESHOLD));

    if (_enabled) {
        // Open-Meteo is the primary weather source for this subsystem.
        fgSetBool("/environment/realwx/enabled", false);
        fgSetBool("/environment/params/metar-updates-environment", false);
        fgSetBool("/environment/params/metar-updates-winds-aloft", false);
    }

    std::string configuredCachePath = _rootNode->getStringValue("cache-file");
    if (!configuredCachePath.empty()) {
        _cachePath = SGPath(configuredCachePath);
    } else {
        _cachePath = globals->get_fg_home();
        _cachePath.append("open-meteo-cache.json");
        _rootNode->setStringValue("cache-file", _cachePath.utf8Str().c_str());
    }

    _writer.reset(new WeatherPropertyWriter(fgGetNode("/environment/config", true),
                                            fgGetNode("/environment", true)));

    if (loadCache()) {
        SG_LOG(SG_ENVIRONMENT, SG_INFO, "Loaded cached Open-Meteo response from " << _cachePath);
        applyCurrentWeather(currentTimeUtc(), 0.0);
    }
}

void OpenMeteoManager::shutdown()
{
    resetLightningState();
    _activeRequest.reset();
}

void OpenMeteoManager::update(double dt)
{
    if (dt < 0.0) {
        return;
    }

    _enabled = _rootNode->getBoolValue("enabled", _enabled);
    if (!_enabled) {
        resetLightningState();
        return;
    }

    _refreshIntervalSec = std::max(MIN_FETCH_INTERVAL_SEC,
                                   _rootNode->getDoubleValue("refresh-seconds", _refreshIntervalSec));
    _refreshDistanceM = std::max(MIN_FETCH_DISTANCE_M,
                                 _rootNode->getDoubleValue("refresh-distance-m", _refreshDistanceM));
    _retryIntervalSec = std::max(MIN_RETRY_INTERVAL_SEC,
                                 _rootNode->getDoubleValue("retry-seconds", _retryIntervalSec));

    const std::time_t nowUtc = currentTimeUtc();

    processCompletedRequest(nowUtc);

    if (_data) {
        applyCurrentWeather(nowUtc, dt);
    }

    const SGGeod currentPosition = globals->get_aircraft_position();
    if (shouldFetch(currentPosition, nowUtc)) {
        startFetch(currentPosition, nowUtc);
    }
}

bool OpenMeteoManager::shouldFetch(const SGGeod& currentPosition, std::time_t nowUtc) const
{
    if (_activeRequest.valid()) {
        return false;
    }

    if (nowUtc < _nextRetryTimeUtc) {
        return false;
    }

    if (!_hasSuccessfulFetch) {
        return true;
    }

    const double elapsedSinceLastFetch = std::difftime(nowUtc, _lastSuccessfulFetchTimeUtc);
    if (elapsedSinceLastFetch >= _refreshIntervalSec) {
        return true;
    }

    const double distanceM = SGGeodesy::distanceM(currentPosition, _fetchLocation);
    return distanceM >= _refreshDistanceM;
}

void OpenMeteoManager::startFetch(const SGGeod& position, std::time_t nowUtc)
{
    auto http = globals->get_subsystem<FGHTTPClient>();
    if (!http) {
        handleFetchFailure(nowUtc, "Open-Meteo HTTP subsystem unavailable");
        return;
    }

    _pendingFetchLocation = position;
    _pendingFetchTimeUtc = nowUtc;
    _activeRequest = new OpenMeteoRequest(position.getLatitudeDeg(),
                                          position.getLongitudeDeg(),
                                          nowUtc);

    simgear::HTTP::Request_ptr request = _activeRequest;
    http->makeRequest(request);

    _rootNode->setDoubleValue("pending-fetch-time-sec", static_cast<double>(nowUtc));
    _rootNode->setDoubleValue("pending-fetch-location/latitude-deg", position.getLatitudeDeg());
    _rootNode->setDoubleValue("pending-fetch-location/longitude-deg", position.getLongitudeDeg());
}

void OpenMeteoManager::processCompletedRequest(std::time_t nowUtc)
{
    if (!_activeRequest.valid()) {
        return;
    }

    const auto state = _activeRequest->readyState();
    if ((state != simgear::HTTP::Request::DONE) &&
        (state != simgear::HTTP::Request::FAILED) &&
        (state != simgear::HTTP::Request::CANCELLED)) {
        return;
    }

    if ((state == simgear::HTTP::Request::DONE) && (_activeRequest->responseCode() == 200)) {
        const std::string body = _activeRequest->responseBody();
        auto parsedData = OpenMeteoData::parse(body);
        if (parsedData) {
            _data = std::move(parsedData);
            _fetchLocation = _pendingFetchLocation;
            _lastSuccessfulFetchTimeUtc = nowUtc;
            _hasSuccessfulFetch = true;
            _nextRetryTimeUtc = 0;

            _rootNode->setBoolValue("using-cache", false);
            _rootNode->setStringValue("last-error", "");
            _rootNode->setDoubleValue("last-successful-fetch-time-sec",
                                      static_cast<double>(_lastSuccessfulFetchTimeUtc));
            _rootNode->setDoubleValue("fetch-location/latitude-deg", _fetchLocation.getLatitudeDeg());
            _rootNode->setDoubleValue("fetch-location/longitude-deg", _fetchLocation.getLongitudeDeg());

            saveCache(body);
        } else {
            handleFetchFailure(nowUtc, "Failed to parse Open-Meteo response");
        }
    } else {
        std::string reason;
        if (state == simgear::HTTP::Request::DONE) {
            reason = "Open-Meteo request failed with HTTP "
                + std::to_string(_activeRequest->responseCode());
        } else if (state == simgear::HTTP::Request::FAILED) {
            reason = "Open-Meteo request failed";
        } else {
            reason = "Open-Meteo request cancelled";
        }
        handleFetchFailure(nowUtc, reason);
    }

    _activeRequest.reset();
}

void OpenMeteoManager::handleFetchFailure(std::time_t nowUtc, const std::string& reason)
{
    SG_LOG(SG_ENVIRONMENT, SG_WARN, reason);
    _rootNode->setStringValue("last-error", reason.c_str());
    _nextRetryTimeUtc = nowUtc + static_cast<std::time_t>(_retryIntervalSec);

    if (!_data) {
        if (loadCache()) {
            SG_LOG(SG_ENVIRONMENT, SG_INFO,
                   "Open-Meteo: loaded cached weather after fetch failure.");
        } else {
            SG_LOG(SG_ENVIRONMENT, SG_WARN,
                   "Open-Meteo: no cache available after fetch failure.");
        }
    }
}

void OpenMeteoManager::applyCurrentWeather(std::time_t nowUtc, double dt)
{
    if (!_data || !_writer) {
        return;
    }

    const AtmosphericProfile boundaryProfile = _data->buildBoundaryProfile(nowUtc);
    const AtmosphericProfile aloftProfile = _data->buildAloftProfile(nowUtc);
    if (boundaryProfile.empty() || aloftProfile.empty()) {
        SG_LOG(SG_ENVIRONMENT, SG_WARN, "Open-Meteo: profile generation returned empty data.");
        return;
    }

    OpenMeteoSurfaceData surface = _data->surfaceData(nowUtc);
    const std::vector<OpenMeteoCloudLayer> cloudLayers = _data->buildCloudLayers(nowUtc, 5);

    const bool lightningFlash = updateLightningState(dt, surface);

    double icingSeverityNorm = surface.icingSeverityNorm;
    const SGGeod aircraftPosition = globals->get_aircraft_position();
    const AtmosphericLayer aloftAtAircraft = aloftProfile.sample(aircraftPosition.getElevationFt());
    if (std::isfinite(aloftAtAircraft.icingSeverityNorm)) {
        icingSeverityNorm = std::max(icingSeverityNorm, aloftAtAircraft.icingSeverityNorm);
    }

    const double pressureSeaLevelHpa = _data->seaLevelPressureHpa(nowUtc);
    _writer->writeWeather(boundaryProfile,
                          aloftProfile,
                          pressureSeaLevelHpa,
                          surface,
                          cloudLayers,
                          lightningFlash,
                          icingSeverityNorm);

    const double icingThreshold = _rootNode->getDoubleValue("icing-threshold", DEFAULT_ICING_THRESHOLD);
    const bool icingActive = icingSeverityNorm >= icingThreshold;
    fgSetBool("/hazards/icing/wing", icingActive);
    fgSetDouble("/hazards/icing/severity-norm", icingSeverityNorm);

    _rootNode->setDoubleValue("last-profile-update-time-sec", static_cast<double>(nowUtc));
    _rootNode->setDoubleValue("convective-severity-norm", surface.convectiveSeverityNorm);
    _rootNode->setDoubleValue("icing-aircraft-severity-norm", icingSeverityNorm);
    _rootNode->setBoolValue("lightning/flash", lightningFlash);
}

bool OpenMeteoManager::updateLightningState(double dt, const OpenMeteoSurfaceData& surfaceData)
{
    double convectionSeverity = surfaceData.convectiveSeverityNorm;
    if (surfaceData.thunderstormLikely) {
        convectionSeverity = std::max(convectionSeverity, 0.6);
    }

    const double threshold = _rootNode->getDoubleValue("lightning/convection-threshold",
                                                       DEFAULT_LIGHTNING_CONVECTION_THRESHOLD);
    if (convectionSeverity < threshold) {
        resetLightningState();
        return false;
    }

    if (_nextLightningIntervalSec <= 0.0) {
        _nextLightningIntervalSec = SGMiscd::lerp(20.0, 3.0, convectionSeverity)
            * SGMiscd::lerp(0.7, 1.3, randomUnit());
    }

    if (_lightningFlashing) {
        _lightningFlashRemainingSec -= dt;
        if (_lightningFlashRemainingSec > 0.0) {
            return true;
        }

        _lightningFlashing = false;
        _lightningFlashRemainingSec = 0.0;
        _lightningElapsedSec = 0.0;
        _nextLightningIntervalSec = SGMiscd::lerp(20.0, 3.0, convectionSeverity)
            * SGMiscd::lerp(0.7, 1.3, randomUnit());
        return false;
    }

    _lightningElapsedSec += dt;
    if (_lightningElapsedSec < _nextLightningIntervalSec) {
        return false;
    }

    _lightningFlashing = true;
    _lightningFlashRemainingSec = SGMiscd::lerp(0.08, 0.22, randomUnit());
    _lightningElapsedSec = 0.0;

    const double angleRad = randomUnit() * SGMiscd::twopi();
    const double rangeM = SGMiscd::lerp(8000.0, 1000.0, convectionSeverity)
        * SGMiscd::lerp(0.8, 1.2, randomUnit());
    fgSetDouble("/environment/lightning/lightning-pos-x", std::cos(angleRad) * rangeM);
    fgSetDouble("/environment/lightning/lightning-pos-y", std::sin(angleRad) * rangeM);
    fgSetDouble("/environment/lightning/lightning-range", rangeM);
    return true;
}

void OpenMeteoManager::resetLightningState()
{
    _lightningElapsedSec = 0.0;
    _nextLightningIntervalSec = 0.0;
    _lightningFlashing = false;
    _lightningFlashRemainingSec = 0.0;
    fgSetBool("/environment/lightning/flash", false);
}

double OpenMeteoManager::randomUnit()
{
    return static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
}

bool OpenMeteoManager::loadCache()
{
    if (_cachePath.isNull() || !_cachePath.exists()) {
        return false;
    }

    sg_ifstream input(_cachePath);
    if (!input.is_open()) {
        SG_LOG(SG_ENVIRONMENT, SG_WARN, "Open-Meteo: unable to open cache " << _cachePath);
        return false;
    }

    const std::string payload((std::istreambuf_iterator<char>(input)),
                              std::istreambuf_iterator<char>());
    if (payload.empty()) {
        SG_LOG(SG_ENVIRONMENT, SG_WARN, "Open-Meteo: cache file is empty " << _cachePath);
        return false;
    }

    auto parsedData = OpenMeteoData::parse(payload);
    if (!parsedData) {
        SG_LOG(SG_ENVIRONMENT, SG_WARN, "Open-Meteo: cache parse failed.");
        return false;
    }

    _data = std::move(parsedData);
    _rootNode->setBoolValue("using-cache", true);
    return true;
}

void OpenMeteoManager::saveCache(const std::string& payload)
{
    if (_cachePath.isNull() || payload.empty()) {
        return;
    }

    SGPath cacheDir = _cachePath.dirPath();
    if (!cacheDir.isNull() && !cacheDir.exists()) {
        cacheDir.create_dir();
    }

    sg_ofstream output(_cachePath, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        SG_LOG(SG_ENVIRONMENT, SG_WARN, "Open-Meteo: unable to write cache file " << _cachePath);
        return;
    }

    output << payload;
}

std::time_t OpenMeteoManager::currentTimeUtc() const
{
    if (globals->get_time_params()) {
        return globals->get_time_params()->get_cur_time();
    }

    return std::time(nullptr);
}

} // namespace Environment
