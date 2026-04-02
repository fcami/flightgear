
/*
 * SPDX-FileCopyrightText: (C) 2016 Edward d'Auvergne
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <algorithm>
#include <iomanip>

#include <cppunit/SourceLine.h>
#include <cppunit/TestResultCollector.h>

#include "fgCompilerOutputter.hxx"
#include "formatting.hxx"
#include "logging.hxx"

using namespace std;


// Create a new class instance.
fgCompilerOutputter * fgCompilerOutputter::defaultOutputter(CppUnit::TestResultCollector *result, vector<TestDataCapt> *capt, const clock_t *clock, CppUnit::OStream &stream)
{
    return new fgCompilerOutputter(result, capt, clock, stream);
}


// Printout for each failed test.
void fgCompilerOutputter::printFailureDetail(CppUnit::TestFailure *failure)
{
    // Declarations.
    TestDataCapt test_data;
    vector<TestDataCapt>::iterator test_iter;

    // Initial separator.
#ifdef _WIN32
    fg_stream << endl;
#endif
    fg_stream << string(WIDTH_DIVIDER, '=') << endl;

    // Test info.
    fg_stream << (failure->isError() ? "ERROR: " : "FAIL: ") << failure->failedTestName() << endl;
    fg_stream << string(WIDTH_DIVIDER, '-') << endl;
    fg_stream << (failure->isError() ? "Error" : "Assertion") << ": ";
    printFailureLocation(failure->sourceLine());
    printFailureMessage(failure);
    fg_stream.flush();

    if (debug)
        return;

    // The captured IO for this test.
    test_iter = find_if(test_data_records->begin(), test_data_records->end(), matchTestName(failure->failedTestName()));
    if (test_iter != test_data_records->end())
        test_data = *test_iter;

    // SG_LOG IO streams.
    if (!test_data.sg_interleaved.empty())
        fgCompilerOutputter::printIOStreamMessages("SG_LOG, interleaved", test_data.sg_interleaved, true);
    if (!test_data.sg_bulk_only.empty())
        fgCompilerOutputter::printIOStreamMessages("SG_LOG, SG_BULK only priority", test_data.sg_bulk_only);
    if (!test_data.sg_debug_only.empty())
        fgCompilerOutputter::printIOStreamMessages("SG_LOG, SG_DEBUG only priority", test_data.sg_debug_only);
    if (!test_data.sg_info_only.empty())
        fgCompilerOutputter::printIOStreamMessages("SG_LOG, SG_INFO only priority", test_data.sg_info_only);
    if (!test_data.sg_warn_only.empty())
        fgCompilerOutputter::printIOStreamMessages("SG_LOG, SG_WARN only priority", test_data.sg_warn_only);
    if (!test_data.sg_alert_only.empty())
        fgCompilerOutputter::printIOStreamMessages("SG_LOG, SG_ALERT only priority", test_data.sg_alert_only);

    // Default IO streams.
    fgCompilerOutputter::printIOStreamMessages("STDOUT and STDERR", test_data.stdio);
}


// Detailed printout after a failed run of the test suite.
void fgCompilerOutputter::printFailureReport()
{
    // Custom printouts for each failed test.
    printFailuresList();

    // CTest output (nothing).
    if (ctest_output)
        return;

    // A summary with timing info.
    printSuiteStats();

    // Final summary.
    fg_stream << endl << "[ FAILED ]" << endl << endl;
    fg_stream.flush();
}


void fgCompilerOutputter::printIOStreamMessages(string heading, string messages, bool empty)
{
    // Silence.
    if (!empty && messages.size() == 0)
        return;

    // Divider.
    fg_stream << string(WIDTH_DIVIDER, '-') << endl;

    // Heading.
    fg_stream << "# " << heading << endl << endl;

    // Nothing to do
    if (messages.size() == 0)
        fg_stream << "(empty)" << endl << endl;

    // The IO stream contents.
    else
        fg_stream << messages << endl;
}


// Printout of the test suite stats.
void fgCompilerOutputter::printSuiteStats()
{
    // A divider.
#ifdef _WIN32
    fg_stream << endl;
#endif
    fg_stream << string(WIDTH_DIVIDER, '-') << endl;

    // Timing and test count line.
    fg_stream << "Ran " << fg_result->runTests() << " tests";
    streamsize prec = fg_stream.precision();
    fg_stream << setprecision(3);
    fg_stream << " in " << ((double)*suite_timer)/CLOCKS_PER_SEC << " seconds." << endl;
    fg_stream << setprecision(prec);

    // Failure line.
    if (!fg_result->wasSuccessful()) {
        fg_stream << endl << "Failures = " << fg_result->testFailures() << endl;
        fg_stream << "Errors   = " << fg_result->testErrors() << endl;
    }
}


// Print a summary after a successful run of the test suite.
void fgCompilerOutputter::printSuccess()
{
    // CTest output (nothing).
    if (ctest_output)
        return;

    // A summary with timing info.
    printSuiteStats();

    // Final summary.
    fg_stream << endl << "[ OK ]" << endl << endl;
    fg_stream.flush();
}
