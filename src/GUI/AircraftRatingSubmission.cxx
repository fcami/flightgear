// AircraftRatingSubmission.cxx - submit a user rating to the ratings backend
//
// SPDX-FileCopyrightText: 2026 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "AircraftRatingSubmission.hxx"

#include <nlohmann/json.hpp>

#include <simgear/io/HTTPRequest.hxx>
#include <simgear/package/Root.hxx>

#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Main/sentryIntegration.hxx>

namespace flightgear {

namespace {

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

} // namespace

void submitAircraftRating(const std::string& qualifiedPackageId, int rating)
{
    const std::string baseUrl = fgGetString("/sim/package-system/ratings-service/url");
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
