// sentryIntegration.hxx - Interface with Sentry.io crash reporting
//
// SPDX-FileCopyrightText: (C) 2018 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <simgear/misc/strutils.hxx>
#include <string>

namespace flightgear {

void initSentry(bool quiet);

void shutdownSentry();

void delayedSentryInit();

bool isSentryEnabled();

void addSentryBreadcrumb(const std::string& msg, const std::string& level);

void addSentryTag(const char* tag, const char* value);

void addSentryTag(const std::string& tag, const std::string& value);

void updateSentryTag(const std::string& tag, const std::string& value);


void sentryReportNasalError(const std::string& msg, const string_list& stack);

void sentryReportException(const std::string& msg, const std::string& location = {});

void sentryReportFatalError(const std::string& msg, const std::string& more = {});

/**
 * information about a specific error / failure, to be sent as part of a user-facing
 * error.
 */
struct SentryExceptionData {
    SentryExceptionData(const std::string& type, const std::string& value, const std::string& location)
        : type(type), value(value), location(location) {}

    std::string type;
    std::string value;
    std::string location;
};

void sentryReportUserError(const std::string& aggregate, const std::string& parameter,
                           const std::vector<SentryExceptionData>& exceptions);

/**
 * @brief retrive the anonymous user ID (a UUID) for this installation.
 *
 * The UUID is generated on first-run and stored in FG_HOME in a text file.  
 */
std::string sentryUserId();

} // namespace flightgear
