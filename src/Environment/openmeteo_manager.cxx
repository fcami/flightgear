/*
 * SPDX-FileCopyrightText: 2026 The FlightGear Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "openmeteo_manager.hxx"

#include "atmospheric_profile.hxx"
#include "openmeteo_data.hxx"
#include "weather_property_writer.hxx"

#include <algorithm>
#include <ctime>
#include <sstream>
#include <utility>

#include <simgear/debug/logstream.hxx>
#include <simgear/io/HTTPRequest.hxx>
#include <simgear/io/iostreams/sgstream.hxx>
#include <simgear/timing/sg_time.hxx>

#include <Main/globals.hxx>
#include <Main/fg_props.hxx>
#include <Network/HTTPClient.hxx>

namespace {

constexpr double MIN_FETCH_INTERVAL_SEC = 1.0;
constexpr double MIN_FETCH_DISTANCE_M = 1.0;
constexpr double MIN_RETRY_INTERVAL_SEC = 1.0;

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

    if (_enabled) {
        // Open-Meteo is the primary source in this pipeline.
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

    _writer.reset(new WeatherPropertyWriter(fgGetNode("/environment/config", true)));

    SG_LOG(SG_ENVIRONMENT, SG_INFO,
           "[TEMP][OpenMeteoManager] init enabled=" << (_enabled ? "true" : "false")
           << " refresh=" << _refreshIntervalSec << "s"
           << " distance=" << _refreshDistanceM << "m"
           << " retry=" << _retryIntervalSec << "s"
           << " cache=" << _cachePath);

    if (loadCache()) {
        SG_LOG(SG_ENVIRONMENT, SG_INFO, "Loaded cached Open-Meteo response from " << _cachePath);
        applyCurrentWeather(currentTimeUtc());
    }
}

void OpenMeteoManager::shutdown()
{
    _activeRequest.reset();
}

void OpenMeteoManager::update(double dt)
{
    if (dt < 0.0) {
        return;
    }

    _enabled = _rootNode->getBoolValue("enabled", _enabled);
    if (!_enabled) {
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
        applyCurrentWeather(nowUtc);
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
        SG_LOG(SG_ENVIRONMENT, SG_INFO,
               "[TEMP][OpenMeteoManager] scheduling initial Open-Meteo fetch");
        return true;
    }

    const double elapsedSinceLastFetch = std::difftime(nowUtc, _lastSuccessfulFetchTimeUtc);
    if (elapsedSinceLastFetch >= _refreshIntervalSec) {
        SG_LOG(SG_ENVIRONMENT, SG_INFO,
               "[TEMP][OpenMeteoManager] scheduling fetch due to refresh interval elapsed="
               << elapsedSinceLastFetch << "s");
        return true;
    }

    const double distanceM = SGGeodesy::distanceM(currentPosition, _fetchLocation);
    if (distanceM >= _refreshDistanceM) {
        SG_LOG(SG_ENVIRONMENT, SG_INFO,
               "[TEMP][OpenMeteoManager] scheduling fetch due to movement distance="
               << distanceM << "m");
        return true;
    }

    return false;
}

void OpenMeteoManager::startFetch(const SGGeod& position, std::time_t nowUtc)
{
    auto http = globals->get_subsystem<FGHTTPClient>();
    if (!http) {
        handleFetchFailure(nowUtc, "HTTP subsystem unavailable");
        return;
    }

    _pendingFetchLocation = position;
    _pendingFetchTimeUtc = nowUtc;
    _activeRequest = new OpenMeteoRequest(position.getLatitudeDeg(),
                                          position.getLongitudeDeg(),
                                          nowUtc);

    SG_LOG(SG_ENVIRONMENT, SG_INFO,
           "[TEMP][OpenMeteoManager] startFetch lat=" << position.getLatitudeDeg()
           << " lon=" << position.getLongitudeDeg()
           << " utc=" << static_cast<double>(nowUtc));

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

    SG_LOG(SG_ENVIRONMENT, SG_INFO,
           "[TEMP][OpenMeteoManager] request completed state=" << static_cast<int>(state)
           << " http=" << _activeRequest->responseCode());

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

            SG_LOG(SG_ENVIRONMENT, SG_INFO,
                   "[TEMP][OpenMeteoManager] parsed response bytes=" << body.size()
                   << " fetchLat=" << _fetchLocation.getLatitudeDeg()
                   << " fetchLon=" << _fetchLocation.getLongitudeDeg());

            saveCache(body);
        } else {
            SG_LOG(SG_ENVIRONMENT, SG_WARN,
                   "[TEMP][OpenMeteoManager] parse failed for payload bytes=" << body.size());
            handleFetchFailure(nowUtc, "Failed to parse Open-Meteo response");
        }
    } else {
        std::ostringstream reason;
        if (state == simgear::HTTP::Request::DONE) {
            reason << "Open-Meteo request failed with HTTP " << _activeRequest->responseCode();
        } else if (state == simgear::HTTP::Request::FAILED) {
            reason << "Open-Meteo request failed";
        } else {
            reason << "Open-Meteo request cancelled";
        }
        handleFetchFailure(nowUtc, reason.str());
    }

    _activeRequest.reset();
}

void OpenMeteoManager::handleFetchFailure(std::time_t nowUtc, const std::string& reason)
{
    SG_LOG(SG_ENVIRONMENT, SG_WARN, reason);
    _rootNode->setStringValue("last-error", reason.c_str());
    _nextRetryTimeUtc = nowUtc + static_cast<std::time_t>(_retryIntervalSec);

    SG_LOG(SG_ENVIRONMENT, SG_INFO,
           "[TEMP][OpenMeteoManager] retry scheduled for utc="
           << static_cast<double>(_nextRetryTimeUtc));

    if (!_data) {
        if (loadCache()) {
            SG_LOG(SG_ENVIRONMENT, SG_INFO,
                   "[TEMP][OpenMeteoManager] loaded cached weather after fetch failure");
        } else {
            SG_LOG(SG_ENVIRONMENT, SG_WARN,
                   "[TEMP][OpenMeteoManager] no cache available after fetch failure");
        }
    }
}

void OpenMeteoManager::applyCurrentWeather(std::time_t nowUtc)
{
    if (!_data || !_writer) {
        return;
    }

    const AtmosphericProfile boundaryProfile = _data->buildBoundaryProfile(nowUtc);
    const AtmosphericProfile aloftProfile = _data->buildAloftProfile(nowUtc);

    if (boundaryProfile.empty() || aloftProfile.empty()) {
        SG_LOG(SG_ENVIRONMENT, SG_WARN,
               "[TEMP][OpenMeteoManager] skipping apply because a profile is empty");
        return;
    }

    const double pressureSeaLevelHpa = _data->seaLevelPressureHpa(nowUtc);
    _writer->writeProfiles(boundaryProfile, aloftProfile, pressureSeaLevelHpa);

    SG_LOG(SG_ENVIRONMENT, SG_INFO,
           "[TEMP][OpenMeteoManager] applied weather boundaryLayers="
           << boundaryProfile.size()
           << " aloftLayers=" << aloftProfile.size()
           << " pressureMSL=" << pressureSeaLevelHpa);

    _rootNode->setDoubleValue("last-profile-update-time-sec", static_cast<double>(nowUtc));
}

bool OpenMeteoManager::loadCache()
{
    if (_cachePath.isNull() || !_cachePath.exists()) {
        SG_LOG(SG_ENVIRONMENT, SG_WARN,
               "[TEMP][OpenMeteoManager] cache file unavailable: " << _cachePath);
        return false;
    }

    sg_ifstream input(_cachePath);
    if (!input.is_open()) {
        SG_LOG(SG_ENVIRONMENT, SG_WARN,
               "[TEMP][OpenMeteoManager] failed to open cache file: " << _cachePath);
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string payload = buffer.str();
    if (payload.empty()) {
        SG_LOG(SG_ENVIRONMENT, SG_WARN,
               "[TEMP][OpenMeteoManager] cache file empty: " << _cachePath);
        return false;
    }

    auto parsedData = OpenMeteoData::parse(payload);
    if (!parsedData) {
        SG_LOG(SG_ENVIRONMENT, SG_WARN,
               "[TEMP][OpenMeteoManager] cache parse failed bytes=" << payload.size());
        return false;
    }

    _data = std::move(parsedData);
    _rootNode->setBoolValue("using-cache", true);
    SG_LOG(SG_ENVIRONMENT, SG_INFO,
           "[TEMP][OpenMeteoManager] loaded cache bytes=" << payload.size());
    return true;
}

void OpenMeteoManager::saveCache(const std::string& payload)
{
    if (_cachePath.isNull()) {
        SG_LOG(SG_ENVIRONMENT, SG_WARN,
               "[TEMP][OpenMeteoManager] saveCache skipped because cache path is null");
        return;
    }

    if (payload.empty()) {
        SG_LOG(SG_ENVIRONMENT, SG_WARN,
               "[TEMP][OpenMeteoManager] saveCache skipped because payload is empty");
        return;
    }

    SGPath cacheDir = _cachePath.dirPath();
    if (!cacheDir.isNull() && !cacheDir.exists()) {
        cacheDir.create_dir();
    }

    sg_ofstream output(_cachePath, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        SG_LOG(SG_ENVIRONMENT, SG_WARN, "Unable to write Open-Meteo cache: " << _cachePath);
        return;
    }

    output << payload;
    SG_LOG(SG_ENVIRONMENT, SG_INFO,
           "[TEMP][OpenMeteoManager] wrote cache bytes=" << payload.size()
           << " path=" << _cachePath);
}

std::time_t OpenMeteoManager::currentTimeUtc() const
{
    if (globals->get_time_params()) {
        return globals->get_time_params()->get_cur_time();
    }

    return std::time(nullptr);
}

} // namespace Environment

