// AircraftRatingTracker.hxx - track "flown long enough to rate" for the current aircraft
//
// SPDX-FileCopyrightText: 2026 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace flightgear {

/**
 * @brief Start the one-shot timer which, 120 seconds after the splash
 * screen has finished hiding, marks the current aircraft's package (if it
 * corresponds to an installed simgear::pkg::Package) as eligible for the
 * user to submit a rating for. Safe to call more than once per session;
 * only the first call actually starts the timer.
 */
void startAircraftRatingFlownTimer();

} // namespace flightgear
