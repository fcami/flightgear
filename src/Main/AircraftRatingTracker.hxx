// AircraftRatingTracker.hxx - track "flown long enough to rate" for the current aircraft
//
// SPDX-FileCopyrightText: 2026 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <simgear/package/PackageCommon.hxx>

namespace flightgear {

/**
 * @brief Start the repeating timer which, once per minute after the splash
 * screen has finished hiding, credits the currently-flown aircraft's
 * package (if it corresponds to an installed simgear::pkg::Package) with
 * one more flown minute (simgear::pkg::Package::addFlownMinutes()). Once
 * enough minutes have accumulated, the package becomes eligible for the
 * user to submit a rating for (see flightgear::canRateAircraft()). Safe
 * to call more than once per session; only the first call actually starts
 * the timer.
 */
void startAircraftRatingFlownTimer();

/**
 * @brief has this package been flown for long enough that we should offer
 * the user a chance to rate it, and has it not been rated yet?
 */
bool canRateAircraft(const simgear::pkg::PackageRef& pkg);

/**
 * @brief POST a rating submission to the configured ratings backend (see
 * fgprop /sim/package-system/ratings-service/url). A no-op if no backend URL
 * is configured. The rating is only recorded locally (via
 * simgear::pkg::Package::setUserRating(), which notifies
 * simgear::pkg::Delegate::packageDataChanged()) once the backend confirms
 * the submission with an HTTP 200 response; on any failure (network error,
 * timeout, non-200 response) the local package data is left untouched.
 */
void submitAircraftRating(const simgear::pkg::PackageRef& pkg, int rating);

} // namespace flightgear
