// AircraftRatingSubmission.hxx - submit a user rating to the ratings backend
//
// SPDX-FileCopyrightText: 2026 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

namespace flightgear {

/**
 * @brief POST a rating submission to the configured ratings backend (see
 * fgprop /sim/package-system/ratings-service/url). A no-op if no backend URL
 * is configured. Fire-and-forget: failures (including an unreachable
 * backend) are simply logged, since the rating has already been recorded
 * locally via simgear::pkg::Root::setUserRatingForPackage() by the time
 * this is called.
 */
void submitAircraftRating(const std::string& qualifiedPackageId, int rating);

} // namespace flightgear
