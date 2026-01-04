/*
* SPDX-FileComment: Flight Gear top level initialization routines
* SPDX-License-Identifier: GPL-2.0-or-later
* SPDX-FileCopyrightText: 1997 Curtis L. Olson
*/

#pragma once

#include <string>
#include <optional>

#include <simgear/misc/sg_path.hxx>

// forward decls
class SGPropertyNode;
class SGPath;

// Return the current base package version
std::string fgBasePackageVersion(const SGPath& path);

/**
 * @brief structure holding parsed info from <base-package>/base_package.json
 *
 */
struct FGBasePackageInfo {
    std::string version;
    std::string suffix;
    std::string buildDate;
    std::string gitRevision;
};

/**
 * @brief Parse the base package info JSON
 *
 * @param path
 * @return std::optional<FGBasePackageInfo>
 */
std::optional<FGBasePackageInfo>
fgBasePackageInfo(const SGPath& path);

SGPath fgHomePath();

enum InitHomeResult
{
    InitHomeOkay,
    InitHomeReadOnly,
    InitHomeExplicitReadOnly,
    InitHomeAbort
};

InitHomeResult fgInitHome(int argc, char** argv);
void fgShutdownHome();
void fgDeleteLockFile();

// Read in configuration (file and command line)
int fgInitConfig ( int argc, char **argv, bool reinit );

void fgInitAircraftPaths(bool reinit);

/**
 * @brief
 *
 * @param reinit : is this a second(+) call of the function, i.e after reset
 * @param didUseLauncher : allow adjusting UI feedback if we used the launcher or not
 * @return int : an Options result to indicate if we should continue, quit, etc
 */
int fgInitAircraft(bool reinit, bool didUseLauncher);

// log various settings / configuration state
void fgOutputSettings();

// Initialize the localization
SGPropertyNode *fgInitLocale(const char *language);

// Init navaids and waypoints
bool fgInitNav ();


// General house keeping initializations
bool fgInitGeneral ();


// Create all the subsystems needed by the sim
void fgCreateSubsystems(bool duringReset);

// called after the subsystems have been bound and initialised,
// to perform final init
void fgPostInitSubsystems();

// Re-position: when only location is changing, we can do considerably
// less work than a full re-init.
void fgStartReposition();

void fgStartNewReset();

// setup the package system including the global root
void fgInitPackageRoot();

// wipe FG_HOME. (The removing of the program data is assumed to be done
// by the real installer).
int fgUninstall();

// flightgear specific exit(status) function
void fgExit(int status);
