/*
 * SPDX-FileCopyrightText: (C) 2016 Edward d'Auvergne
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <algorithm>
#include <cstring>
#include <iostream>

#include <simgear/debug/logstream.hxx>
#include <simgear/misc/strutils.hxx>

#include "dataStore.hxx"
#include "fgTestRunner.hxx"
#include "formatting.hxx"
#include "logging.hxx"

using namespace std;


// The help message.
void helpPrintout(std::ostream &stream) {
    stream << "Usage: fgfs_test_suite [options]\n";
    stream << '\n';
    stream << "Options:\n";
    stream << "  -h, --help            show this help message and exit.\n";
    stream << '\n';
    stream << "  Test selection options:\n";
    stream << "    -s, --system-tests  execute the system/functional tests.\n";
    stream << "    -u, --unit-tests    execute the unit tests.\n";
    stream << "    -g, --gui-tests     execute the GUI tests.\n";
    stream << "    -m, --simgear-tests execute the simgear tests.\n";
    stream << "    -f, --fgdata-tests  execute the FGData tests.\n";
    stream << '\n';
    stream << "    The -s, -u, -g, and -m options accept an optional argument to perform a\n";
    stream << "    subset of all tests.  This argument should either be the name of a test\n";
    stream << "    suite, the full name of an individual test, or a comma separated list.\n";
    stream << '\n';
    stream << "    Full test names consist of the test suite name, the separator '::' and\n";
    stream << "    then the individual test name.  The test names can revealed with the -t\n";
    stream << "    option.\n";
    stream << '\n';
    stream << "    Examples:\n";
    stream << "      - perform and print the names of all available unit tests:\n";
    stream << "        --> fgfs_test_suite -t -u\n";
    stream << "      - run all tests from the NavRadioTests test suite:\n";
    stream << "        --> fgfs_test_suite -u NavRadioTests\n";
    stream << "      - run a specific test without discarding its output:\n";
    stream << "        --> fgfs_test_suite --no-summary -d -u NavRadioTests::testGS\n";
    stream << '\n';
    stream << "  Logging options:\n";
    stream << "    --log <log_spec>    define logging as a list of category=level tuples\n";
    stream << "                        eg atc=off,terrain=warn,all=info\n";
    stream << "    --log-split         output the different non-interleaved log streams\n";
    stream << "                        sequentially.\n";
    stream << '\n';
    stream << "  Verbosity options:\n";
    stream << "    -t, --timings       verbose output including names and timings for all\n";
    stream << "                        tests.\n";
    stream << "    -c, --ctest         simplified output suitable for running via CTest.\n";
    stream << "    -d, --debug         disable IO capture for debugging (super verbose output).\n";
    stream << "    --no-summary        disable the final summary printout.\n";
    stream << '\n';
    stream << "  FG options:\n";
    stream << "    --fg-root           the path to FGData.\n";
    stream << '\n';
    stream << "Environmental variables:\n";
    stream << "  FG_TEST_LOGGING       equivalent to the --log option.\n";
    stream << "  FG_TEST_LOG_SPLIT     equivalent to the --log-split option.\n";
    stream << "  FG_TEST_TIMINGS       equivalent to the -t or --timings option.\n";
    stream << "  FG_TEST_DEBUG         equivalent to the -d or --debug option.\n";
    stream << "  FG_ROOT               the path to FGData.  The order of precedence is\n";
    stream << "                         --fg-root, the FG_DATA_DIR CMake option, FG_ROOT,\n";
    stream << "                        '../fgdata/', and '../data/'.\n";
    stream.flush();
}


// Print out a summary of the relax test suite.
void summary(CppUnit::OStream &stream, int system_result, int unit_result, int gui_result, int simgear_result, int fgdata_result)
{
    int synopsis = 0;

    // Title.
    string text = "Summary of the FlightGear test suite";
    printTitle(stream, text);

    // Subtitle.
    text = "Synopsis";
    printSection(stream, text);

    // System/functional test summary.
    if (system_result != -1) {
        text = "System/functional tests";
        printSummaryLine(stream, text, system_result);
        synopsis += system_result;
    }

    // Unit test summary.
    if (unit_result != -1) {
        text = "Unit tests";
        printSummaryLine(stream, text, unit_result);
        synopsis += unit_result;
    }

    // GUI test summary.
    if (gui_result != -1) {
        text = "GUI tests";
        printSummaryLine(stream, text, gui_result);
        synopsis += gui_result;
    }

    // Simgear unit test summary.
    if (simgear_result != -1) {
        text = "Simgear unit tests";
        printSummaryLine(stream, text, simgear_result);
        synopsis += simgear_result;
    }

    // FGData test summary.
    if (fgdata_result != -1) {
        text = "FGData tests";
        printSummaryLine(stream, text, fgdata_result);
        synopsis += fgdata_result;
    }

    // Synopsis.
    text ="Synopsis";
    printSummaryLine(stream, text, synopsis);

    // End.
    stream << endl << endl;
}


int main(int argc, char **argv)
{
    // Declarations.
    int         status_gui=-1, status_simgear=-1, status_system=-1, status_unit=-1, status_fgdata=-1;
    bool        run_system=false, run_unit=false, run_gui=false, run_simgear=false, run_fgdata=false;
    bool        logSplit=false;
    bool        timings=false, ctest_output=false, junit_output=false, debug=false, printSummary=true, help=false;
    char        *subset_system=NULL, *subset_unit=NULL, *subset_gui=NULL, *subset_simgear=NULL, *subset_fgdata=NULL;
    bool        failure=false;
    char        firstchar;
    std::string arg, delim, fgRoot, logClassVal, logLevel;
    size_t      delimPos;

    simgear::LogLevels logLevels;
    logLevels.set(SG_ALL, SG_INFO);

    // Process environmental variables before the command line options.
    if (getenv("FG_TEST_LOGGING")) {
        auto ll = simgear::parseLogSpecFromString(getenv("FG_TEST_LOGGING"));
        if (!ll) {
            return 1;
        }
        logLevels = *ll;
    }

    if (getenv("FG_TEST_LOG_SPLIT"))
        logSplit = true;
    if (getenv("FG_TEST_TIMINGS"))
        timings = true;
    if (getenv("FG_TEST_DEBUG"))
        debug = true;
    if (failure)
        return 1;

    // Argument parsing.
    for (int i = 1; i < argc; i++) {
        firstchar = '\0';
        arg = argv[i];

        if (i < argc-1)
            firstchar = argv[i+1][0];

        // System test.
        if (arg == "-s" || arg == "--system-tests") {
            run_system = true;
            if (firstchar != '-')
                subset_system = argv[i+1];

        // Unit test.
        } else if (arg == "-u" || arg == "--unit-tests") {
            run_unit = true;
            if (firstchar != '-')
                subset_unit = argv[i+1];

        // GUI test.
        } else if (arg == "-g" || arg == "--gui-tests") {
            run_gui = true;
            if (firstchar != '-')
                subset_gui = argv[i+1];

        // Simgear test.
        } else if (arg == "-m" || arg == "--simgear-tests") {
            run_simgear = true;
            if (firstchar != '-')
                subset_simgear = argv[i+1];

        // FGData test.
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fgdata-tests") == 0) {
            run_fgdata = true;
            if (firstchar != '-')
                subset_fgdata = argv[i+1];

        // Log class.
        } else if (arg.find("--logging") == 0) {
            // Process the command line.
            auto ll = simgear::parseLogSpecFromString(arg);
            if (!ll) {
                return 1;
            }

            logLevels = *ll;

            // Log splitting.
        } else if (arg == "--log-split") {
            logSplit = true;

        // Timing output.
        } else if (arg == "-t" || arg == "--timings") {
            timings = true;

        // CTest suitable output.
        } else if (arg == "-c" || arg == "--ctest") {
            ctest_output = true;

        // JUnit suitable output.
        } else if (arg == "-j" || arg == "--junit") {
            junit_output = true;

        // Debug output.
        } else if (arg == "-d" || arg == "--debug") {
            debug = true;

        // No summary output.
        } else if (arg == "--no-summary") {
            printSummary = false;

        // Help.
        } else if (arg == "-h" || arg == "--help") {
            help = true;

        // FGData path.
        } else if (arg == "--fg-root") {
            if (firstchar != '-')
                fgRoot = argv[i+1];
        }
    }

    // Help.
    if (help) {
        helpPrintout(std::cout);
        return 0;
    }

    // Turn on all tests if no subset was specified.
    if (!run_system && !run_unit && !run_gui && !run_simgear && !run_fgdata) {
        run_system = true;
        run_unit = true;
        run_gui = true;
        run_simgear = true;
        run_fgdata = true;
    }

    // Set up the data store singleton and FGData path.
    DataStore& data = DataStore::get();
    if (data.findFGRoot(fgRoot, debug) != 0) {
        return 1;
    }
    if (data.validateFGRoot() != 0) {
        return 1;
    }

    // Set up logging.
    sglog().setDeveloperMode(true);
    if (debug)
        sglog().setLogLevels(logLevels);
    else
        setupLogging(logLevels, logSplit);

    // Execute each of the test suite categories.
    if (run_system)
        status_system = testRunner("System tests", "System / functional tests", subset_system, timings, ctest_output, junit_output, debug);
    if (run_unit)
        status_unit = testRunner("Unit tests", "Unit tests", subset_unit, timings, ctest_output, junit_output, debug);
    if (run_gui && 0) // Disabled as there are no GUI tests yet.
        status_gui = testRunner("GUI tests", "GUI tests", subset_gui, timings, ctest_output, junit_output, debug);
    if (run_simgear)
        status_simgear = testRunner("Simgear unit tests", "Simgear unit tests", subset_simgear, timings, ctest_output, junit_output, debug);
    if (run_fgdata)
        status_fgdata = testRunner("FGData tests", "FGData tests", subset_fgdata, timings, ctest_output, junit_output, debug);

    // Summary printout.
    if (printSummary && !ctest_output)
        summary(cerr, status_system, status_unit, status_gui, status_simgear, status_fgdata);

    // Deactivate the logging.
    if (!debug)
        stopLogging();

    // Failure.
    if (status_system > 0)
        return 1;
    if (status_unit > 0)
        return 1;
    if (status_gui > 0)
        return 1;
    if (status_simgear > 0)
        return 1;
    if (status_fgdata > 0)
        return 1;

    // Success.
    return 0;
}
