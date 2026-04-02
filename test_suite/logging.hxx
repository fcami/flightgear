/*
 * SPDX-FileCopyrightText: (C) 2016 Edward d'Auvergne
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <mutex>
#include <sstream>

#include <simgear/debug/LogCallback.hxx>


// All of the captured IO streams.
class capturedIO : public simgear::LogCallback
{
public:
    // Constructor and destructor.
    capturedIO();
    ~capturedIO();

    bool doProcessEntry(const simgear::LogEntry& e) override;

    void setSplit(bool split) { _split = split; }

    // The IO streams.
    std::ostringstream sg_interleaved;

    std::ostringstream sg_bulk_only;
    std::ostringstream sg_debug_only;
    std::ostringstream sg_info_only;
    std::ostringstream sg_warn_only;
    std::ostringstream sg_alert_only;

    bool _split = false;

    std::mutex log_capture_lock;
};


// Return the global stream capture data structure, creating it if needed.
capturedIO& getIOstreams();

// Set up to capture all the simgear logging priorities as separate streams.
void setupLogging(const simgear::LogLevels levels, bool split);

// Deactivate all the simgear logging priority IO captures.
void stopLogging();
