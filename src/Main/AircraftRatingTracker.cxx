// AircraftRatingTracker.cxx - track "flown long enough to rate" for the current aircraft
//
// SPDX-FileCopyrightText: 2026 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AircraftRatingTracker.hxx"

#include <nlohmann/json.hpp>

#include <simgear/io/HTTPRequest.hxx>
#include <simgear/package/Package.hxx>
#include <simgear/package/Root.hxx>
#include <simgear/structure/event_mgr.hxx>

#include "fg_props.hxx"
#include "globals.hxx"
#include "sentryIntegration.hxx"

namespace flightgear {

namespace {

bool timerStarted = false;

/// minimum accumulated flown minutes before we consider offering the user a
/// chance to rate a package
const uint32_t MIN_FLOWN_MINUTES_TO_RATE = 0;

const std::string DEFAULT_RATINGS_SERVICE_URL = "https://ratings.flightgear.org";

/**
 * Short connect/total timeouts so an unreachable/down backend fails fast
 * rather than hanging for curl's multi-minute defaults - see
 * simgear::HTTP::Request::connectTimeoutMsec()/totalTimeoutMsec().
 */
class RatingSubmissionRequest : public simgear::HTTP::Request
{
public:
    RatingSubmissionRequest(const std::string& url) : simgear::HTTP::Request(url, "POST")
    {
        requestHeader("User-Agent") = "FlightGear-Launcher-Ratings/1";
    }

    long connectTimeoutMsec() const override { return 8000; }
    long totalTimeoutMsec() const override { return 15000; }
};

/// re-reads the current aircraft each time it fires, so it naturally
/// tracks flown time for whichever aircraft is active when each minute
/// elapses (including across in-sim aircraft changes)
void onFlownMinuteElapsed()
{
    auto pkgRoot = globals->packageRoot();
    if (!pkgRoot) {
        return;
    }

    auto pkg = pkgRoot->getPackageById(fgGetString("/sim/aircraft-id"));
    if (!pkg) {
        return; // not an installed package, eg a locally-added aircraft
    }

    pkg->addFlownMinutes(1);
}

} // namespace

void startAircraftRatingFlownTimer()
{
    if (timerStarted) {
        return;
    }

    timerStarted = true;
    globals->get_event_mgr()->addTask(
        "aircraft-rating-flown-timer", &onFlownMinuteElapsed,
        60.0 /* interval */, 60.0 /* delay */);
}

bool canRateAircraft(const simgear::pkg::PackageRef& pkg)
{
    if (!pkg) {
        return false;
    }

    return (pkg->flownMinutes() >= MIN_FLOWN_MINUTES_TO_RATE) && (pkg->userRating() < 0);
}

void submitAircraftRating(const std::string& qualifiedPackageId, int rating)
{
    const std::string baseUrl = fgGetString("/sim/package-system/ratings-service/url",
        DEFAULT_RATINGS_SERVICE_URL);
    if (baseUrl.empty()) {
        return; // no backend configured, nothing to do
    }

    nlohmann::json body;
    body["aircraft_id"] = qualifiedPackageId;
    body["rating"] = rating;
    body["anonymous_id"] = sentryUserId();

    auto req = new RatingSubmissionRequest(baseUrl + "/api/v1/ratings");
    req->setBodyData(body.dump(), "application/json");
    globals->packageRoot()->makeHTTPRequest(req);
}

} // namespace flightgear
