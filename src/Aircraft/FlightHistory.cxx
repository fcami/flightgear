// FlightHistory
//
// SPDX-FileCopyrightText: Copyright (C) 2012 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "FlightHistory.hxx"

#include <algorithm>

#include <chrono>
#include <cstdint>
#include <simgear/debug/logstream.hxx>
#include <simgear/math/SGMath.hxx>
#include <simgear/misc/strutils.hxx>
#include <simgear/props/props_io.hxx>
#include <simgear/sg_inlines.h>
#include <simgear/structure/commands.hxx>
#include <simgear/structure/exception.hxx>

#include <Main/fg_props.hxx>
#include <Main/globals.hxx>

using namespace std::chrono;

FGFlightHistory::FGFlightHistory() = default;

FGFlightHistory::~FGFlightHistory() = default;

void FGFlightHistory::init()
{
    m_enabled = fgGetNode("/sim/history/enabled", true);
    m_sampleInterval = fgGetDouble("/sim/history/sample-interval-sec", 1.0);
    if (m_sampleInterval <= 0.0) { // would be bad
        SG_LOG(SG_FLIGHT, SG_INFO, "invalid flight-history sample interval:" << m_sampleInterval
               << ", defaulting to " << m_sampleInterval);
        m_sampleInterval = 1.0;
    }

  // cap memory use at 4MB
    m_maxMemoryUseBytes = fgGetInt("/sim/history/max-memory-use-bytes", 1024 * 1024 * 4);
    m_weightOnWheels = NULL;
// reset the history when we detect a take-off
    if (fgGetBool("/sim/history/clear-on-takeoff", true)) {
        m_weightOnWheels = fgGetNode("/gear/gear[1]/wow", 0, true);
        m_lastWoW = m_weightOnWheels->getBoolValue();
    }

    // force bucket re-allocation
    m_lastCaptureTime = globals->get_sim_time_sec();

    globals->get_commands()->addCommand("clear-flight-history", this, &FGFlightHistory::clearHistoryCommand);
}

void FGFlightHistory::shutdown()
{
    clear();
    globals->get_commands()->removeCommand("clear-flight-history");
}

void FGFlightHistory::reinit()
{
    shutdown();
    init();
}

void FGFlightHistory::update(double dt)
{
    if ((dt == 0.0) || !m_enabled->getBoolValue()) {
        return; // paused or disabled
    }

    if (m_weightOnWheels) {

        if (m_lastWoW && !m_weightOnWheels->getBoolValue()) {
            SG_LOG(SG_FLIGHT, SG_INFO, "history: detected main-gear takeoff, clearing history");
            clear();
        }
    } // of rest-on-takeoff enabled

// spatial check - moved at least 1m since last capture
    if (!m_buckets.empty()) {
        const auto& cb = currentBucket();
        if (!cb.isEmpty()) {
            SGVec3d lastCaptureCart(SGVec3d::fromGeod(cb.lastSample().position));
            double d2 = distSqr(lastCaptureCart, globals->get_aircraft_position_cart());
            if (d2 <= 1.0) {
                return;
            }
        }
    }

    double elapsed = globals->get_sim_time_sec() - m_lastCaptureTime;
    if (elapsed > m_sampleInterval) {
        capture();
    }
}

void FGFlightHistory::allocateNewBucket()
{
    if (!m_buckets.empty() && (currentMemoryUseBytes() > m_maxMemoryUseBytes)) {
        m_buckets.pop_front();
    }

    m_buckets.emplace_back();
}

FGFlightHistory::SampleBucket& FGFlightHistory::currentBucket()
{
    assert(!m_buckets.empty());
    return m_buckets.back();
}


void FGFlightHistory::capture()
{
    if (m_buckets.empty() || currentBucket().isComplete()) {
        allocateNewBucket();
    }

    m_lastCaptureTime = globals->get_sim_time_sec();
    auto& cb = currentBucket();
    auto& sample = cb.samples[cb.validSamples];

    sample.simTimeMSec = static_cast<size_t>(m_lastCaptureTime * 1000.0);
    sample.position = globals->get_aircraft_position();

    double heading, pitch, roll;
    globals->get_aircraft_orientation(heading, pitch, roll);
    sample.heading = static_cast<float>(heading);
    sample.pitch = static_cast<float>(pitch);
    sample.roll = static_cast<float>(roll);

    cb.validSamples++;
}

PagedPathForHistory_ptr FGFlightHistory::pagedPathForHistory(size_t max_entries, size_t newerThan ) const
{
    PagedPathForHistory_ptr result = new PagedPathForHistory();
    if (m_buckets.empty()) {
        return result;
    }

    for (const auto& bucket : m_buckets) {
        // iterate over all the valid samples in the bucket
        for (unsigned int index = 0; index < bucket.validSamples; ++index) {
            // skip older entries
            // TODO: bisect!
            if (bucket.samples[index].simTimeMSec <= newerThan)
                continue;

            if( max_entries ) {
                max_entries--;
                SGGeod g = bucket.samples[index].position;
                result->path.push_back(g);
                result->last_seen = bucket.samples[index].simTimeMSec;
            } else {
                goto exit;
            }

        } // of samples iteration
    } // of buckets iteration

exit:
    return result;
}


SGGeodVec FGFlightHistory::pathForHistory(double minEdgeLengthM) const
{
    SGGeodVec result;
    if (m_buckets.empty()) {
        return result;
    }

    result.push_back(m_buckets.front().samples[0].position);
    SGVec3d lastOutputCart = SGVec3d::fromGeod(result.back());
    double minLengthSqr = minEdgeLengthM * minEdgeLengthM;

    for (const auto& bucket : m_buckets) {
        const unsigned int count = bucket.validSamples;

        // iterate over all the valid samples in the bucket
        for (unsigned int index = 0; index < count; ++index) {
            SGGeod g = bucket.samples[index].position;
            SGVec3d cart(SGVec3d::fromGeod(g));
            if (distSqr(cart, lastOutputCart) > minLengthSqr) {
                lastOutputCart =  cart;
                result.push_back(g);
            }
        } // of samples iteration
    } // of buckets iteration

    return result;
}

void FGFlightHistory::clear()
{
    m_buckets.clear();
}

size_t FGFlightHistory::SampleBucket::bucketMinAge() const
{
    // youngest / most recent (minimum) age in the bucket is the last value
    if (validSamples == 0) {
        return {};
    }

    return samples[validSamples - 1].simTimeMSec;
}

const FGFlightHistory::Sample& FGFlightHistory::SampleBucket::lastSample() const
{
    if (validSamples == 0) {
        return samples.front();
    }

    return samples.at(validSamples - 1);
}

void FGFlightHistory::clearOlderThan(std::chrono::seconds keepMostRecent)
{
    if (keepMostRecent.count() == 0) {
        clear();
        return;
    }

    const auto cutoff = globals->get_sim_time_sec() - keepMostRecent.count();
    const auto cutoffMSec = static_cast<size_t>(cutoff * 1000.0);
    while (!m_buckets.empty()) {
        if (m_buckets.front().bucketMinAge() >= cutoffMSec) {
            break;
        }

        m_buckets.pop_front(); // clear oldest bucket
    }

    // we don't worry about doing a partial clear of front-most bucket
}

size_t FGFlightHistory::currentMemoryUseBytes() const
{
    return sizeof(SampleBucket) * m_buckets.size();
}

bool FGFlightHistory::clearHistoryCommand(const SGPropertyNode* args, SGPropertyNode*)
{
    const auto keepSeconds = args->getDoubleValue("keep-most-recent-secs", 0.0);
    clearOlderThan(std::chrono::seconds(static_cast<int64_t>(keepSeconds)));
    return true;
}


// Register the subsystem.
SGSubsystemMgr::Registrant<FGFlightHistory> registrantFGFlightHistory;
