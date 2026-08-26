// AircraftRatingTracker.cxx - track "flown long enough to rate" for the current aircraft
//
// SPDX-FileCopyrightText: 2026 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AircraftRatingTracker.hxx"

#include <set>

#include <nlohmann/json.hpp>

#include <simgear/debug/logstream.hxx>
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
const uint32_t MIN_FLOWN_MINUTES_TO_RATE = 5;

const std::string DEFAULT_RATINGS_SERVICE_URL = "https://ratings.flightgear.org";

/// qualified package IDs with a rating submission currently in flight -
/// used to hide the rating UI (see canRateAircraft()) and so prevent the
/// user from firing off a second, duplicate POST while one is pending
std::set<std::string> pendingRatingSubmissions;

/// safety-net delay before we give up waiting on a rating submission and
/// re-allow the user to retry, in case the request never completes at all
/// (ie RatingSubmissionRequest::onAlways() is never called). Comfortably
/// longer than RatingSubmissionRequest::totalTimeoutMsec() below.
const double PENDING_RATING_SAFETY_TIMEOUT_SEC = 20.0;

std::string pendingRatingTimerName(const std::string& qualifiedId)
{
    return "aircraft-rating-submission-timeout-" + qualifiedId;
}

void clearPendingRatingSubmission(const std::string& qualifiedId)
{
    pendingRatingSubmissions.erase(qualifiedId);
    globals->get_event_mgr()->removeTask(pendingRatingTimerName(qualifiedId));

    // refresh 'can rate' UI state, in case the submission did not succeed
    auto pkgRoot = globals->packageRoot();
    auto pkg = pkgRoot ? pkgRoot->getPackageById(qualifiedId) : simgear::pkg::PackageRef();
    if (pkg) {
        pkg->notifyUserDataChanged();
    }
}

/**
 * Short connect/total timeouts so an unreachable/down backend fails fast
 * rather than hanging for curl's multi-minute defaults - see
 * simgear::HTTP::Request::connectTimeoutMsec()/totalTimeoutMsec().
 */
class RatingSubmissionRequest : public simgear::HTTP::Request
{
public:
    RatingSubmissionRequest(const std::string& url, const simgear::pkg::PackageRef& pkg, int rating) : simgear::HTTP::Request(url, "POST"),
                                                                                                       m_package(pkg),
                                                                                                       m_rating(rating)
    {
        requestHeader("User-Agent") = "FlightGear-Launcher-Ratings/1";
    }

    long connectTimeoutMsec() const override { return 8000; }
    long totalTimeoutMsec() const override { return 15000; }

protected:
    void onDone() override
    {
        if (responseCode() == 200) {
            // notifies simgear::pkg::Delegate::packageDataChanged() listeners
            m_package->setUserRating(m_rating);
            SG_LOG(SG_AIRCRAFT, SG_DEBUG, "aircraft rating submission for " << m_package->qualifiedId() << " succeeded");
        } else {
            SG_LOG(SG_IO, SG_WARN, "aircraft rating submission for " << m_package->qualifiedId() << " failed: HTTP " << responseCode());
        }
    }

    void onAlways() override
    {
        // covers the DONE, FAILED and CANCELLED cases alike
        clearPendingRatingSubmission(m_package->qualifiedId());
    }

private:
    simgear::pkg::PackageRef m_package;
    int m_rating;
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

    if (!pkg->isInstalled()) {
        return false;
    }

    if (pendingRatingSubmissions.count(pkg->qualifiedId()) > 0) {
        return false; // a submission is already in flight for this package
    }

    return (pkg->flownMinutes() >= MIN_FLOWN_MINUTES_TO_RATE) && (pkg->userRating() < 0);
}

void submitAircraftRating(const simgear::pkg::PackageRef& pkg, int rating)
{
    if (!pkg) {
        return;
    }

    const auto qualifiedId = pkg->qualifiedId();
    if (pendingRatingSubmissions.count(qualifiedId) > 0) {
        return; // already submitting a rating for this package, ignore
    }

    const std::string baseUrl = fgGetString("/sim/package-system/ratings-service/url",
                                            DEFAULT_RATINGS_SERVICE_URL);
    if (baseUrl.empty()) {
        return; // no backend configured, nothing to do
    }

    pendingRatingSubmissions.insert(qualifiedId);
    globals->get_event_mgr()->addEvent(
        pendingRatingTimerName(qualifiedId),
        [qualifiedId]() { clearPendingRatingSubmission(qualifiedId); },
        PENDING_RATING_SAFETY_TIMEOUT_SEC);

    // hide the rating UI immediately, rather than waiting for the request
    // to complete
    pkg->notifyUserDataChanged();

    nlohmann::json body;
    body["aircraft_id"] = qualifiedId;
    body["rating"] = rating;
    body["anonymous_id"] = sentryUserId();

    auto req = new RatingSubmissionRequest(baseUrl + "/api/v1/ratings", pkg, rating);
    req->setBodyData(body.dump(), "application/json");
    globals->packageRoot()->makeHTTPRequest(req);
}

} // namespace flightgear
