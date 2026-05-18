// SPDX-FileCopyrightText: 2016 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <functional>
#include <vector>
#include <optional>

#include <simgear/math/sg_types.hxx>
#include <simgear/math/SGGeod.hxx>
#include <simgear/props/propsfwd.hxx>
#include <simgear/structure/SGSharedPtr.hxx>

namespace flightgear
{
    class FlightPlan;
    typedef SGSharedPtr<FlightPlan> FlightPlanRef;

    typedef std::vector<SGGeod> SGGeodVec;
}

namespace FGTestApi {

namespace setUp {

void initTestGlobals(const std::string& testName,
                     const std::string& language = "default",
                     bool initFGLocale = true);

bool logPositionToKML(const std::string& testName);
/**Don't log aircraft positions*/
bool logLinestringsToKML(const std::string& testName);

void initStandardNasal(bool withCanvas = false);

void populateFPWithoutNasal(flightgear::FlightPlanRef f,
                         const std::string& depICAO, const std::string& depRunway,
                         const std::string& destICAO, const std::string& destRunway,
                         const std::string& waypoints);

void populateFPWithNasal(flightgear::FlightPlanRef f,
            const std::string& depICAO, const std::string& depRunway,
            const std::string& destICAO, const std::string& destRunway,
            const std::string& waypoints);

}  // End of namespace setUp.

// helpers during tests

SGPropertyNode_ptr propsFromString(const std::string& s);

const SGGeod getPosition();
void setPosition(const SGGeod& g);
void setPositionAndStabilise(const SGGeod& g);

void runForTime(double t);

/**
 @brief set the simulation date/time clock to 'time'
 */
void adjustSimulationWorldTime(time_t time);

using RunCheck = std::function<bool(void)>;

bool runForTimeWithCheck(double t, RunCheck check);

void writeFlightPlanToKML(flightgear::FlightPlanRef fp);

void writeGeodsToKML(const std::string &label, const flightgear::SGGeodVec& geods);
void writePointToKML(const std::string& ident, const SGGeod& pos);

/** Run nasal code.
 *
 * Return false in case of parse or runtime error.
 * lineOffset is passed to the Nasal parser so that line numbers in error
 * messages refer to actual source lines in the calling file.
 */
bool executeNasal(const std::string& code, int lineOffset = 1);

/** Run nasal code and return runtime errors.
 *
 * In case of parse error, return empty optional.
 * Otherwise, return the list of runtime errors.
 */
std::optional<string_list> executeNasalExpectRuntimeErrors(const std::string& code, int lineOffset = 1);

/**
 * Convenience macros that automatically pass the current source line so that
 * Nasal error messages show the correct location in the C++ test file.
 */
#define FGTESTAPI_EXECUTE_NASAL(code) \
    FGTestApi::executeNasal((code), __LINE__)

#define FGTESTAPI_EXECUTE_NASAL_EXPECT_ERRORS(code) \
    FGTestApi::executeNasalExpectRuntimeErrors((code), __LINE__)

/**
 * @brief parse some Nasal expecting a parser error.
 *
 * return the parser error message, or empty if no error occurs
 */
std::string parseNasalExpectError(const std::string& code);

bool geodsApproximatelyEqual(const SGGeod& a, const SGGeod& b);

namespace tearDown {

void shutdownTestGlobals();

}  // End of namespace tearDown.

}  // End of namespace FGTestApi.
