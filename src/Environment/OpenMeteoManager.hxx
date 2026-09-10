/*
 * SPDX-FileCopyrightText: 2026 The FlightGear Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <ctime>
#include <memory>
#include <string>

#include <simgear/math/SGMath.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/props/props.hxx>
#include <simgear/structure/subsystem_mgr.hxx>

#include "OpenMeteoRequest.hxx"

namespace Environment {

class OpenMeteoData;
struct OpenMeteoSurfaceData;
class WeatherPropertyWriter;

class OpenMeteoManager : public SGSubsystem
{
public:
    explicit OpenMeteoManager(SGPropertyNode_ptr rootNode);
    ~OpenMeteoManager() override;

    void init() override;
    void shutdown() override;
    void update(double dt) override;

    static const char* staticSubsystemClassId() { return "open-meteo"; }

private:
    bool shouldFetch(const SGGeod& currentPosition, std::time_t nowUtc) const;
    void startFetch(const SGGeod& position, std::time_t nowUtc);
    void processCompletedRequest(std::time_t nowUtc);
    void handleFetchFailure(std::time_t nowUtc, const std::string& reason);

    void applyCurrentWeather(std::time_t nowUtc, double dt);

    bool updateLightningState(double dt, const OpenMeteoSurfaceData& surfaceData);
    void resetLightningState();
    static double randomUnit();

    bool loadCache();
    void saveCache(const std::string& payload);

    std::time_t currentTimeUtc() const;

    SGPropertyNode_ptr _rootNode;
    bool _enabled = true;
    double _refreshIntervalSec = 900.0;
    double _refreshDistanceM = 1000.0;
    double _retryIntervalSec = 60.0;

    SGPath _cachePath;

    std::unique_ptr<OpenMeteoData> _data;
    std::unique_ptr<WeatherPropertyWriter> _writer;

    OpenMeteoRequestRef _activeRequest;
    SGGeod _pendingFetchLocation;
    std::time_t _pendingFetchTimeUtc = 0;

    SGGeod _fetchLocation;
    std::time_t _lastSuccessfulFetchTimeUtc = 0;
    std::time_t _nextRetryTimeUtc = 0;
    bool _hasSuccessfulFetch = false;

    double _lightningElapsedSec = 0.0;
    double _nextLightningIntervalSec = 0.0;
    bool _lightningFlashing = false;
    double _lightningFlashRemainingSec = 0.0;
};

} // namespace Environment

