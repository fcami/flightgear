// AircraftRatingTracker.cxx - track "flown long enough to rate" for the current aircraft
//
// SPDX-FileCopyrightText: 2026 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AircraftRatingTracker.hxx"

#include <simgear/package/Package.hxx>
#include <simgear/package/Root.hxx>
#include <simgear/structure/event_mgr.hxx>

#include "fg_props.hxx"
#include "globals.hxx"

namespace flightgear {

namespace {

bool timerStarted = false;

void onFlownEnoughToRate(const std::string& aircraftIdAtStart)
{
    if (aircraftIdAtStart.empty()) {
        return;
    }

    // only mark the aircraft if it's still the one being flown - avoids
    // mis-attributing the flag if the user has changed aircraft since
    // startup (eg via a in-sim aircraft reset/reload)
    if (fgGetString("/sim/aircraft-id") != aircraftIdAtStart) {
        return;
    }

    auto pkgRoot = globals->packageRoot();
    if (!pkgRoot) {
        return;
    }

    auto pkg = pkgRoot->getPackageById(aircraftIdAtStart);
    if (!pkg) {
        return; // not an installed package, eg a locally-added aircraft
    }

    pkgRoot->markPackageFlownEnoughToRate(pkg->qualifiedId());
}

} // namespace

void startAircraftRatingFlownTimer()
{
    if (timerStarted) {
        return;
    }

    timerStarted = true;
    const std::string aircraftIdAtStart = fgGetString("/sim/aircraft-id");
    globals->get_event_mgr()->addEvent(
        "aircraft-rating-flown-timer",
        [aircraftIdAtStart]() { onFlownEnoughToRate(aircraftIdAtStart); },
        120.0);
}

} // namespace flightgear
