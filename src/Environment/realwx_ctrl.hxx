// realwx_ctrl.cxx -- Process real weather data
//
// Written by David Megginson, started May 2002.
// Rewritten by Torsten Dreyer, August 2010
//
// SPDX-FileCopyrightText: 2002 David Megginson <david@megginson.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <simgear/props/props.hxx>
#include <simgear/structure/SGSharedPtr.hxx>
#include <simgear/structure/subsystem_mgr.hxx>

#include "metarproperties.hxx"

namespace Environment {

class LiveMetarProperties;
typedef SGSharedPtr<LiveMetarProperties> LiveMetarProperties_ptr;

/**
 * Abstraction over "fetch a METAR for this station id, call back the
 * handler when it arrives (or fails)".
 */
class MetarRequester
{
public:
    virtual ~MetarRequester() = default;
    virtual void requestMetar(LiveMetarProperties_ptr metarDataHandler, const std::string& id) = 0;
};

/**
 * Ties METAR data (station-id, wind, temperature, pressure, ...) to
 * properties under a given root node (see MetarProperties), and fetches
 * fresh data via a MetarRequester on a timer.
 */
class LiveMetarProperties : public MetarProperties
{
public:
    /**
     * @param refreshIntervalSeconds How long a successfully fetched METAR is
     *   considered fresh before update() requests another one (see
     *   handleMetarData()). Defaults to DEFAULT_TIME_TO_LIVE_SECONDS, which
     *   suits the single primary/aircraft station; callers managing many
     *   simultaneous stations (e.g. FGEnvironmentMgr's per-position cache)
     *   should pass a larger, coarser value to avoid refreshing dozens of
     *   airports as often as the one the pilot actually cares about.
     */
    LiveMetarProperties(SGPropertyNode_ptr rootNode, MetarRequester* metarRequester, int maxAge,
                        double refreshIntervalSeconds = DEFAULT_TIME_TO_LIVE_SECONDS);
    virtual ~LiveMetarProperties();
    virtual void update(double dt);

    virtual double getTimeToLive() const { return _timeToLive; }
    virtual void resetTimeToLive()
    {
        _timeToLive = 0.0;
        _pollingTimer = 0.0;
    }

    // implementation of MetarDataHandler
    virtual void handleMetarData(const std::string& data);
    virtual void handleMetarFailure();

    static const unsigned MAX_POLLING_INTERVAL_SECONDS = 10;
    static const unsigned DEFAULT_TIME_TO_LIVE_SECONDS = 900;

private:
    double _timeToLive;
    double _pollingTimer;
    MetarRequester* _metarRequester;
    int _maxAge;
    bool _failure;
    double _refreshIntervalSeconds;
};

class RealWxController : public SGSubsystem
{
public:
    virtual ~RealWxController();

    static RealWxController * createInstance( SGPropertyNode_ptr rootNode );

    /**
     * Expose the underlying METAR fetch mechanism so other subsystems (the
     * per-position environment cache in FGEnvironmentMgr) can drive their
     * own LiveMetarProperties fetchers directly rather than going through
     * property-tree discovery. May return nullptr (e.g. no real-weather
     * source configured).
     */
    virtual MetarRequester* getMetarRequester() = 0;
};

} // namespace Environment
