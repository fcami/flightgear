// sentryIntegration.cxx - Interface with Sentry.io crash reporting
//
// SPDX-FileCopyrightText: 2020 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "sentryIntegration.hxx"

#include <cstring> // for strcmp

#include <simgear/debug/ErrorReportingCallback.hxx>
#include <simgear/debug/LogCallback.hxx>
#include <simgear/debug/Reporting.hxx>
#include <simgear/debug/logstream.hxx>

#include <simgear/io/iostreams/sgstream.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/props/props.hxx>
#include <simgear/structure/commands.hxx>
#include <simgear/structure/exception.hxx>

#include <Main/fg_init.hxx>
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>

#include <flightgearBuildId.h>

bool doesStringMatchPrefixes(const std::string& s, const std::initializer_list<const char*>& prefixes)
{
    if (s.empty())
        return false;


    for (auto c : prefixes) {
        if (s.find(c) == 0)
            return true;
    }

    return false;
}

auto OSG_messageWhitelist = {
    "PNG lib warning : iCCP: known incorrect sRGB profile",
    "PNG lib warning : iCCP: profile 'ICC Profile': 1000000h: invalid rendering intent",
    "osgDB ac3d reader: detected surface with less than 3",
    "osgDB ac3d reader: detected line with less than 2",
    "Detected particle system using segment(s) with less than 2 vertices"};

auto exception_messageWhitelist = {
    "position is invalid, NaNs", ///< avoid spam when NaNs occur
    "bad AI flight plan",        ///< adjusting logic to avoid this is tricky
    "couldn't find shader",      ///< handled separately
    "(EMEXEC)",                  ///< Emesary log spam
    /// suppress noise from user-entered METAR values : we special case
    /// when live metar fails to parse
    "metar data bogus",
    "metar data incomplete",
    "metar temperature data",
    "metar pressure data"};

auto general_messageWhitelist = {
    " -- Recipient",              ///< emmessary overrun message, note leading whitespace
    "Overrun: GlobalTransmitter", ///< emmesary overrun message
};

// we don't want sentry enabled for the test suite
#if defined(HAVE_SENTRY) && !defined(BUILDING_TESTSUITE)

static bool static_sentryEnabled = false;
static std::string static_sentryUUID;

#include <sentry.h>

namespace {

// this callback is invoked whenever an instance of sg_throwable is created.
// therefore we can log the stack trace at that point
void sentryTraceSimgearThrow(const std::string& msg, const std::string& origin, const sg_location& loc)
{
    if (!static_sentryEnabled)
        return;

    if (doesStringMatchPrefixes(msg, exception_messageWhitelist)) {
        return;
    }

    sentry_value_t exc = sentry_value_new_object();
    sentry_value_set_by_key(exc, "type", sentry_value_new_string("Exception"));

    std::string message = msg;
    sentry_value_t info = sentry_value_new_object();
    if (!origin.empty()) {
        sentry_value_set_by_key(info, "origin", sentry_value_new_string(origin.c_str()));
    }

    if (loc.isValid()) {
        const auto ls = loc.asString();
        sentry_value_set_by_key(info, "location", sentry_value_new_string(ls.c_str()));
    }

    sentry_set_context("what", info);
    sentry_value_set_by_key(exc, "value", sentry_value_new_string(message.c_str()));

    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "exception", exc);

    sentry_event_value_add_stacktrace(event, nullptr, 0);
    sentry_capture_event(event);
}

class SentryLogCallback : public simgear::LogCallback
{
public:
    SentryLogCallback() : simgear::LogCallback(SG_ALL, SG_WARN)
    {
    }

    bool doProcessEntry(const simgear::LogEntry& e) override
    {
        // we need original priority here, so we don't record MANDATORY_INFO
        // or DEV_ messages, which would get noisy.
        const auto op = e.originalPriority;
        if ((op != SG_WARN) && (op != SG_ALERT)) {
            return true;
        }

        if ((e.debugClass == SG_OSG) && doesStringMatchPrefixes(e.message, OSG_messageWhitelist)) {
            return true;
        }

        if (doesStringMatchPrefixes(e.message, general_messageWhitelist)) {
            return true;
        }

        if (e.message == _lastLoggedMessage) {
            _lastLoggedCount++;
            return true;
        }

        if (_lastLoggedCount > 0) {
            flightgear::addSentryBreadcrumb("(repeats " + std::to_string(_lastLoggedCount) + " times)", "info");
            _lastLoggedCount = 0;
        }

        _lastLoggedMessage = e.message;
        flightgear::addSentryBreadcrumb(e.message, (op == SG_WARN) ? "warning" : "error");
        return true;
    }

private:
    std::string _lastLoggedMessage;
    int _lastLoggedCount = 0;
};

const auto missingShaderPrefix = std::string{"Missing shader"};

string_list anon_missingShaderList;

bool isNewMissingShader(const std::string& path)
{
    auto it = std::find(anon_missingShaderList.begin(), anon_missingShaderList.end(), path);
    if (it != anon_missingShaderList.end()) {
        return false;
    }

    anon_missingShaderList.push_back(path);
    return true;
}

void sentrySimgearReportCallback(const std::string& msg, const std::string& more, bool isFatal)
{
    // don't duplicate reports for missing shaders, once per sessions
    // is sufficient
    using simgear::strutils::starts_with;
    if (starts_with(msg, missingShaderPrefix)) {
        if (!isNewMissingShader(more)) {
            return;
        }
    }

    sentry_value_t exc = sentry_value_new_object();
    if (isFatal) {
        sentry_value_set_by_key(exc, "type", sentry_value_new_string("Fatal Error"));
    } else {
        sentry_value_set_by_key(exc, "type", sentry_value_new_string("Exception"));
    }

    sentry_value_set_by_key(exc, "value", sentry_value_new_string(msg.c_str()));

    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "exception", exc);

    sentry_event_value_add_stacktrace(event, nullptr, 0);
    sentry_capture_event(event);
}

void sentryReportBadAlloc()
{
    if (simgear::ReportBadAllocGuard::isSet()) {
        sentry_value_t sentryMessage = sentry_value_new_object();
        sentry_value_set_by_key(sentryMessage, "type", sentry_value_new_string("Fatal Error"));
        sentry_value_set_by_key(sentryMessage, "formatted", sentry_value_new_string("bad allocation"));

        sentry_value_t event = sentry_value_new_event();
        sentry_value_set_by_key(event, "message", sentryMessage);

        sentry_event_value_add_stacktrace(event, nullptr, 0);
        sentry_capture_event(event);
    }

    throw std::bad_alloc(); // allow normal processing
}

} // namespace

namespace flightgear {

bool sentryReportCommand(const SGPropertyNode* args, SGPropertyNode* root)
{
    if (!static_sentryEnabled) {
        SG_LOG(SG_GENERAL, SG_WARN, "Sentry.io not enabled at startup");
        return false;
    }

    sentry_value_t exc = sentry_value_new_object();
    sentry_value_set_by_key(exc, "type", sentry_value_new_string("Report"));

    const auto message = args->getStringValue("message");
    sentry_value_set_by_key(exc, "value", sentry_value_new_string(message.c_str()));

    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "exception", exc);
    // capture the C++ stack-trace. Probably not that useful but can't hurt
    sentry_event_value_add_stacktrace(event, nullptr, 0);

    sentry_capture_event(event);

    return true;
}

bool sentrySendError(const SGPropertyNode* args, SGPropertyNode* root)
{
    if (!static_sentryEnabled) {
        SG_LOG(SG_GENERAL, SG_WARN, "Sentry.io not enabled at startup");
        return false;
    }

    try {
        throw sg_io_exception("Invalid flurlbe", sg_location("/Some/dummy/path/bar.txt", 100, 200));
    } catch (sg_exception& e) {
        SG_LOG(SG_GENERAL, SG_WARN, "caught dummy exception");
    }

    return true;
}

std::string sentryUserId()
{
    if (!static_sentryUUID.empty()) {
        return static_sentryUUID;
    }

    const auto uuidPath = fgHomePath() / "sentry_uuid.txt";
    if (!uuidPath.exists()) {
        return {};
    }

    sg_ifstream f(uuidPath);
    std::getline(f, static_sentryUUID);
    return static_sentryUUID;
}

void initSentry(bool quiet)
{
    sentry_options_t* options = sentry_options_new();
    // API key is defined in config.h, set in an environment variable prior
    // to running CMake, so it can be customised. Env var at build time is:
    // FLIGHTGEAR_SENTRY_API_KEY
    sentry_options_set_dsn(options, SENTRY_API_KEY);

    if (strcmp(FG_BUILD_TYPE, "Dev") == 0) {
        sentry_options_set_environment(options, "dev");
        sentry_options_set_release(options, "flightgear-dev@" REVISION);
    } else if (strcmp(FG_BUILD_TYPE, "Nightly") == 0) {
        sentry_options_set_environment(options, "dev");
        // we use the build year and month here to cluster nightly builds in Sentry.
        // the idea being that this is sufficient granularity to notice when an issue
        // first occurs, and then bisect the commits. We could switch to using
        // year and week number, but this makes more noise in sentry.
        sentry_options_set_release(options, "flightgear-nightly@" BUILD_MONTH);
        sentry_options_set_environment(options, "testing");
    } else if (strcmp(FG_BUILD_TYPE, "Release") == 0) {
        // RC builds are for testing
        if (strncmp(BUILD_SUFFIX, "rc", 2) == 0) {
            sentry_options_set_environment(options, "testing");
        } else {
            sentry_options_set_environment(options, "production");
        }

        sentry_options_set_release(options, FLIGHTGEAR_VERSION);
    } else {
        // unknown build type
        sentry_options_set_environment(options, "dev");
    }

    sentry_options_set_dist(options, REVISION);

    // for dev / nightly builds, put Sentry in debug mode
    if (!quiet && strcmp(FG_BUILD_TYPE, "Release")) {
        sentry_options_set_debug(options, 1);
    }

    SGPath dataPath = fgHomePath() / "sentry_db";
#if defined(SG_WINDOWS)
    const auto homePathString = dataPath.wstr();
    sentry_options_set_database_pathw(options, homePathString.c_str());

    const auto logPath = (fgHomePath() / "fgfs.log").wstr();
    sentry_options_add_attachmentw(options, logPath.c_str());
#else
    const auto homePathString = dataPath.utf8Str();
    sentry_options_set_database_path(options, homePathString.c_str());

    const auto logPath = (fgHomePath() / "fgfs.log").utf8Str();
    sentry_options_add_attachment(options, logPath.c_str());
#endif

    const auto uuidPath = fgHomePath() / "sentry_uuid.txt";
    bool generateUuid = true;
    std::string uuid;
    if (uuidPath.exists()) {
        sentryUserId(); // will cache into static_sentryUUID as a side-effect

        // if we read enough bytes, that this is a valid UUID, then accept it
        if (static_sentryUUID.length() >= 36) {
            generateUuid = false;
        }
    }

    // we need to generate a new UUID
    if (generateUuid) {
        // use the Sentry APi to generate one
        sentry_uuid_t su = sentry_uuid_new_v4();
        char bytes[38];
        sentry_uuid_as_string(&su, bytes);
        bytes[37] = 0;

        static_sentryUUID = std::string{bytes};
        // write it back to disk for next time
        sg_ofstream f(uuidPath);
        f << static_sentryUUID << std::endl;
    }

    if (sentry_init(options) == 0) {
        static_sentryEnabled = true;
        sentry_value_t user = sentry_value_new_object();
        sentry_value_t userUuidV = sentry_value_new_string(static_sentryUUID.c_str());
        sentry_value_set_by_key(user, "id", userUuidV);
        sentry_set_user(user);

        sglog().addCallback(new SentryLogCallback);
        setThrowCallback(sentryTraceSimgearThrow);
        simgear::setErrorReportCallback(sentrySimgearReportCallback);

        std::set_new_handler(sentryReportBadAlloc);
    } else {
        SG_LOG(SG_GENERAL, SG_WARN, "Failed to init Sentry reporting");
        static_sentryEnabled = false;
    }
}

void delayedSentryInit()
{
    if (!static_sentryEnabled)
        return;

    // allow the user to opt-out of sentry.io features
    if (!fgGetBool("/sim/startup/sentry-crash-reporting-enabled", true)) {
        SG_LOG(SG_GENERAL, SG_INFO, "Disabling Sentry.io reporting");
        sentry_shutdown();
        static_sentryEnabled = false;
        return;
    }

    globals->get_commands()->addCommand("sentry-report", &sentryReportCommand);
    globals->get_commands()->addCommand("sentry-exception", &sentrySendError);

    // expose the anonymous user UUID to the property tree, so users
    // can share it if they wish
    fgSetString("/sim/crashreport/sentry-user-id", static_sentryUUID);
}

void shutdownSentry()
{
    if (static_sentryEnabled) {
        sentry_shutdown();
        static_sentryEnabled = false;
    }
}

bool isSentryEnabled()
{
    return static_sentryEnabled;
}

void addSentryBreadcrumb(const std::string& msg, const std::string& level)
{
    if (!static_sentryEnabled)
        return;

    sentry_value_t crumb = sentry_value_new_breadcrumb("default", msg.c_str());
    sentry_value_set_by_key(crumb, "level", sentry_value_new_string(level.c_str()));
    sentry_add_breadcrumb(crumb);
}

void addSentryTag(const char* tag, const char* value)
{
    if (!static_sentryEnabled)
        return;

    if (!tag || !value)
        return;

    sentry_set_tag(tag, value);
}

void updateSentryTag(const std::string& tag, const std::string& value)
{
    if (tag.empty() || value.empty())
        return;

    if (!static_sentryEnabled)
        return;

    sentry_remove_tag(tag.c_str());
    sentry_set_tag(tag.c_str(), value.c_str());
}

void sentryReportNasalError(const std::string& msg, const string_list& stack)
{
    if (!static_sentryEnabled)
        return;
#if 0
    sentry_value_t exc = sentry_value_new_object();
    sentry_value_set_by_key(exc, "type", sentry_value_new_string("Exception"));
    sentry_value_set_by_key(exc, "value", sentry_value_new_string(msg.c_str()));

    sentry_value_t stackData = sentry_value_new_list();
    for (const auto& nasalFrame : stack) {
        sentry_value_append(stackData, sentry_value_new_string(nasalFrame.c_str()));
    }
    sentry_value_set_by_key(exc, "stack", stackData);


    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "exception", exc);

    // add the Nasal stack trace data

    // capture the C++ stack-trace. Probably not that useful but can't hurt
    sentry_event_value_add_stacktrace(event, nullptr, 0);

    sentry_capture_event(event);

#endif
}

void sentryReportException(const std::string& msg, const std::string& location)
{
    if (!static_sentryEnabled)
        return;

    sentry_value_t exc = sentry_value_new_object();
    sentry_value_set_by_key(exc, "type", sentry_value_new_string("Exception"));


    sentry_value_t info = sentry_value_new_object();
    if (!location.empty()) {
        sentry_value_set_by_key(info, "location", sentry_value_new_string(location.c_str()));
    }
    sentry_set_context("what", info);

    sentry_value_set_by_key(exc, "value", sentry_value_new_string(msg.c_str()));

    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "exception", exc);

    // capture the C++ stack-trace. Probably not that useful but can't hurt
    sentry_event_value_add_stacktrace(event, nullptr, 0);
    sentry_capture_event(event);
}

void sentryReportFatalError(const std::string& msg, const std::string& more)
{
    if (!static_sentryEnabled)
        return;

    sentry_value_t sentryMessage = sentry_value_new_object();
    sentry_value_set_by_key(sentryMessage, "type", sentry_value_new_string("Fatal Error"));

    sentry_value_t info = sentry_value_new_object();
    if (!more.empty()) {
        sentry_value_set_by_key(info, "more", sentry_value_new_string(more.c_str()));
    }

    sentry_set_context("what", info);
    sentry_value_set_by_key(sentryMessage, "formatted", sentry_value_new_string(msg.c_str()));

    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "message", sentryMessage);

    sentry_event_value_add_stacktrace(event, nullptr, 0);
    sentry_capture_event(event);
}

void sentryReportUserError(const std::string& aggregate, const std::string& parameter, const std::string& details)
{
    if (!static_sentryEnabled)
        return;

    sentry_value_t sentryMessage = sentry_value_new_object();
    sentry_value_set_by_key(sentryMessage, "type", sentry_value_new_string("Error"));

    sentry_value_t info = sentry_value_new_object();
    sentry_value_set_by_key(info, "details", sentry_value_new_string(details.c_str()));

    sentry_set_context("what", info);

    auto m = aggregate;
    if (!parameter.empty()) {
        m += ":" + parameter;
    }

    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "message", sentry_value_new_string(m.c_str()));

    sentry_capture_event(event);
}

} // namespace flightgear

#else

// stubs for non-sentry case

namespace flightgear {

void initSentry()
{
}

void shutdownSentry()
{
}

void delayedSentryInit()
{
}

std::string sentryUserId()
{
    return {};
}

bool isSentryEnabled()
{
    return false;
}

void addSentryBreadcrumb(const std::string&, const std::string&)
{
}

void addSentryTag(const char*, const char*)
{
}

void updateSentryTag(const std::string&, const std::string&)
{
}


void sentryReportNasalError(const std::string&, const string_list&)
{
}

void sentryReportException(const std::string&, const std::string&)
{
}

void sentryReportFatalError(const std::string&, const std::string&)
{
}

void sentryReportUserError(const std::string&, const std::string&, const std::string&)
{
}

} // namespace flightgear

#endif

// common helpers

namespace flightgear {

void addSentryTag(const std::string& tag, const std::string& value)
{
    if (tag.empty() || value.empty())
        return;

    addSentryTag(tag.c_str(), value.c_str());
}

} // namespace flightgear
