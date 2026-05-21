/*
 * SPDX-FileCopyrightText: 2016 Edward d'Auvergne
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <iomanip>

#include "logging.hxx"

#include <simgear/debug/logstream.hxx>
#include <simgear/scene/util/OsgIoCapture.hxx>


// The global stream capture data structure.
static std::unique_ptr<capturedIO> _iostreams;

// capturedIO constructor.
capturedIO::capturedIO() : simgear::LogCallback("test")
{
    sglog().addCallback(this);
}

capturedIO::~capturedIO()
{
    sglog().removeCallback(this);
}

bool capturedIO::doProcessEntry(const simgear::LogEntry& e)
{
    if (!shouldLog(e.debugClass, e.debugPriority)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(log_capture_lock);

    if (_split) {
        std::ostringstream* streamPtr = nullptr;
        // split the message into the appropriate stream
        switch (e.debugPriority) {
        case SG_BULK:
            streamPtr = &sg_bulk_only;
            break;
        case SG_DEBUG:
            streamPtr = &sg_debug_only;
            break;
        case SG_INFO:
            streamPtr = &sg_info_only;
            break;
        case SG_WARN:
            streamPtr = &sg_warn_only;
            break;
        case SG_ALERT:
            streamPtr = &sg_alert_only;
            break;
        default:
            // ignore other priorities
            return false;
        }

        if (streamPtr) {
            *streamPtr << debugClassToString(e.debugClass) << ":" << e.file << ":" << e.line << ": " << e.message << std::endl;
        }
    } else {
        // interleaved stream, include the priority
        sg_interleaved << debugClassToString(e.debugClass) << ":" << (int)e.debugPriority << ":" << e.file << ":" << e.line << ": " << e.message << std::endl;
    }

    return true;
}

// Return the global stream capture data structure, creating it if needed.
capturedIO& getIOstreams()
{
    // Initialise the global stream capture data structure, if needed.
    if (!_iostreams)
        _iostreams.reset(new capturedIO());

    // Return a pointer to the global object.
    return *(_iostreams.get());
}


// Set up to capture all the simgear logging priorities as separate streams.
void setupLogging(const simgear::LogLevels levels, bool split)
{
    // Get the single logstream instance.
    logstream &log = sglog();

    // Set up the logstream testing mode.
    // (removes all existing callbacks)
    log.setTestingMode(true);

    // OSG IO capture.
    osg::setNotifyHandler(new SGNotifyHandler);

    // IO capture.
    getIOstreams();
    _iostreams->setLogLevels(levels);
    _iostreams->setSplit(split);
}


// Deactivate all the simgear logging priority IO captures.
void stopLogging()
{
    _iostreams.reset();

    // Stop the simgear logstream.
    simgear::shutdownLogging();
}
