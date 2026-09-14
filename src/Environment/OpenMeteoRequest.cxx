/*
 * SPDX-FileCopyrightText: 2026 The FlightGear Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "OpenMeteoRequest.hxx"

#include <cstdio>
#include <ctime>
#include <string>

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

    char stamp[18] = {};
    std::snprintf(stamp, sizeof(stamp),
                  "%04d-%02d-%02dT%02d:00",
                  utcTime.tm_year + 1900,
                  utcTime.tm_mon + 1,
                  utcTime.tm_mday,
                  utcTime.tm_hour);
    return stamp;
}

std::string toCompactString(double value)
{
    std::string text = std::to_string(value);

    // Trim trailing zeros while keeping at least one fractional digit when needed.
    const std::size_t dotPos = text.find('.');
    if (dotPos != std::string::npos) {
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }

        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
    }

    return text;
}

void appendPressureLevelVariables(std::string& hourly, int pressureLevelHpa)
{
    const std::string levelSuffix = std::to_string(pressureLevelHpa) + "hPa";

    hourly += ",temperature_" + levelSuffix;
    hourly += ",dew_point_" + levelSuffix;
    hourly += ",relative_humidity_" + levelSuffix;
    hourly += ",cloud_cover_" + levelSuffix;
    hourly += ",wind_speed_" + levelSuffix;
    hourly += ",wind_direction_" + levelSuffix;
    hourly += ",geopotential_height_" + levelSuffix;
    hourly += ",vertical_velocity_" + levelSuffix;
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
    std::string hourly =
        "pressure_msl,surface_pressure,temperature_2m,dew_point_2m,relative_humidity_2m,"
        "visibility,precipitation,rain,showers,snowfall,weather_code,cloud_cover,"
        "cloud_cover_low,cloud_cover_mid,cloud_cover_high,wind_speed_10m,wind_direction_10m,"
        "wind_gusts_10m,wind_speed_80m,wind_direction_80m,cape,lifted_index,"
        "convective_inhibition,freezing_level_height";

    for (int pressureLevelHpa : PRESSURE_LEVELS) {
        appendPressureLevelVariables(hourly, pressureLevelHpa);
    }

    const std::string requestedHour = formatIsoHourUtc(queryTimeUtc);
    const std::time_t systemNowUtc = std::time(nullptr);
    const bool useArchiveEndpoint = std::difftime(queryTimeUtc, systemNowUtc) < -(16.0 * 24.0 * 60.0 * 60.0);
    const char* endpoint = useArchiveEndpoint
        ? "https://archive-api.open-meteo.com/v1/archive"
        : "https://api.open-meteo.com/v1/forecast";

    std::string url(endpoint);
    url += "?latitude=" + toCompactString(latitudeDeg);
    url += "&longitude=" + toCompactString(longitudeDeg);
    url += "&hourly=" + hourly;
    url += "&wind_speed_unit=kn";
    url += "&start_hour=" + requestedHour;
    url += "&end_hour=" + requestedHour;
    url += "&timezone=GMT";
    url += "&cell_selection=nearest";
    return url;
}

} // namespace Environment

