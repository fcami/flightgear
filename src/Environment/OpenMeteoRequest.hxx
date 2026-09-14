/*
 * SPDX-FileCopyrightText: 2026 The FlightGear Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <ctime>
#include <string>

#include <simgear/io/HTTPMemoryRequest.hxx>

namespace Environment {

class OpenMeteoRequest : public simgear::HTTP::MemoryRequest
{
public:
    OpenMeteoRequest(double latitudeDeg, double longitudeDeg, std::time_t queryTimeUtc);

    static std::string buildUrl(double latitudeDeg, double longitudeDeg, std::time_t queryTimeUtc);
};

typedef SGSharedPtr<OpenMeteoRequest> OpenMeteoRequestRef;

} // namespace Environment

