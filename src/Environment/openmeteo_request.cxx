/*
 * SPDX-FileCopyrightText: 2026 The FlightGear Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "openmeteo_request.hxx"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace {

constexpr int PRESSURE_LEVELS[] = {
    1000, 975, 950, 925, 900, 850, 800, 750, 700, 650,
    600, 550, 500, 450, 400, 350, 300, 250, 200
};

std::string formatIsoHourUtc(std::time_t queryTimeUtc)
{
    std::tm utcTime = {};
#if defined(_MSC_VER)
    gmtime_s(&utcTime, &queryTimeUtc);
#else
    const std::tm* utc = std::gmtime(&queryTimeUtc);
    if (utc) {
        utcTime = *utc;
    }
#endif

    std::ostringstream out;
    out << std::setfill('0')
        << std::setw(4) << (utcTime.tm_year + 1900)
        << '-'
        << std::setw(2) << (utcTime.tm_mon + 1)
        << '-'
        << std::setw(2) << utcTime.tm_mday
        << 'T'
        << std::setw(2) << utcTime.tm_hour
        << ":00";
    return out.str();
}

} // namespace

namespace Environment {

OpenMeteoRequest::OpenMeteoRequest(double latitudeDeg,
                                   double longitudeDeg,
                                   std::time_t queryTimeUtc) :
    simgear::HTTP::MemoryRequest(buildUrl(latitudeDeg, longitudeDeg, queryTimeUtc))
{
}

std::string OpenMeteoRequest::buildUrl(double latitudeDeg,
                                       double longitudeDeg,
                                       std::time_t queryTimeUtc)
{
    std::ostringstream hourly;
    hourly << "pressure_msl"
           << ",temperature_2m"
           << ",wind_speed_10m"
           << ",wind_direction_10m"
           << ",wind_speed_80m"
           << ",wind_direction_80m"
           << ",relative_humidity_2m"
           << ",cloud_cover";

    for (int levelHpa : PRESSURE_LEVELS) {
        hourly << ",temperature_" << levelHpa << "hPa";
        hourly << ",wind_speed_" << levelHpa << "hPa";
        hourly << ",wind_direction_" << levelHpa << "hPa";
        hourly << ",geopotential_height_" << levelHpa << "hPa";
        hourly << ",relative_humidity_" << levelHpa << "hPa";
        hourly << ",cloud_cover_" << levelHpa << "hPa";
    }

    const std::string requestedHour = formatIsoHourUtc(queryTimeUtc);
    const std::time_t systemNowUtc = std::time(nullptr);
    const bool useArchiveEndpoint = std::difftime(queryTimeUtc, systemNowUtc) < -(16.0 * 24.0 * 60.0 * 60.0);
    const char* endpoint = useArchiveEndpoint
        ? "https://archive-api.open-meteo.com/v1/archive"
        : "https://api.open-meteo.com/v1/forecast";

    std::ostringstream url;
    url << std::fixed << std::setprecision(6);
    url << endpoint
        << "?latitude=" << latitudeDeg
        << "&longitude=" << longitudeDeg
        << "&hourly=" << hourly.str()
        << "&wind_speed_unit=kn"
        << "&start_hour=" << requestedHour
        << "&end_hour=" << requestedHour
        << "&timezone=GMT"
        << "&cell_selection=nearest";

    return url.str();
}

} // namespace Environment

