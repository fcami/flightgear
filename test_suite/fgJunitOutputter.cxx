/*
 * SPDX-FileName: fgJunitOutputter.cxx
 * SPDX-FileComment: Logger for JUnit modelled along https://gitlab.com/gitlab-org/gitlab/-/blob/master/lib/gitlab/ci/parsers/test/junit.rb
 * SPDX-FileCopyrightText: Copyright (C) 2016 Edward d'Auvergne, 2025 Keith Paterson
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <cppunit/SourceLine.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/tools/XmlDocument.h>
#include <cppunit/tools/XmlElement.h>

#include "fgJunitOutputter.hxx"
#include "formatting.hxx"
#include "logging.hxx"

namespace fs = std::filesystem;
using namespace std;


// Create a new class instance.
fgJunitOutputter* fgJunitOutputter::defaultOutputter(const std::string& type, CppUnit::TestResultCollector* result, vector<TestDataCapt>* capt, const clock_t* clock, CppUnit::OStream& stream)
{
    return new fgJunitOutputter(type, result, capt, clock, stream);
}


// Printout for each failed test.
void fgJunitOutputter::printFailureDetail(CppUnit::TestFailure* failure)
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

    std::string filename = failure->sourceLine().fileName().replace(0, failure->sourceLine().fileName().find("test_suite"), "");
    // #L141
    test_data.fileName = filename + "#L" + std::to_string(failure->sourceLine().lineNumber());
    test_data.failureText = getMessage(failure->thrownException()->message());

    *test_iter = test_data;

    // SG_LOG IO streams.
    if (!test_data.sg_interleaved.empty())
        fgJunitOutputter::printIOStreamMessages("SG_LOG, interleaved", test_data.sg_interleaved, true);
    if (!test_data.sg_bulk_only.empty())
        fgJunitOutputter::printIOStreamMessages("SG_LOG, SG_BULK only priority", test_data.sg_bulk_only);
    if (!test_data.sg_debug_only.empty())
        fgJunitOutputter::printIOStreamMessages("SG_LOG, SG_DEBUG only priority", test_data.sg_debug_only);
    if (!test_data.sg_info_only.empty())
        fgJunitOutputter::printIOStreamMessages("SG_LOG, SG_INFO only priority", test_data.sg_info_only);
    if (!test_data.sg_warn_only.empty())
        fgJunitOutputter::printIOStreamMessages("SG_LOG, SG_WARN only priority", test_data.sg_warn_only);
    if (!test_data.sg_alert_only.empty())
        fgJunitOutputter::printIOStreamMessages("SG_LOG, SG_ALERT only priority", test_data.sg_alert_only);

    // Default IO streams.
    fgJunitOutputter::printIOStreamMessages("STDOUT and STDERR", test_data.stdio);
}


void fgJunitOutputter::printIOStreamMessages(string heading, string messages, bool empty)
{
    // Silence.
    if (!empty && messages.size() == 0)
        return;

    // Divider.
    fg_stream << string(WIDTH_DIVIDER, '-') << endl;

    // Heading.
    fg_stream << "# " << heading << endl
              << endl;

    // Nothing to do
    if (messages.size() == 0)
        fg_stream << "(empty)" << endl
                  << endl;

    // The IO stream contents.
    else
        fg_stream << messages << endl;
}


// Printout of the test suite stats.
void fgJunitOutputter::printStatistics()
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
    fg_stream << " in " << ((double)*suite_timer) / CLOCKS_PER_SEC << " seconds." << endl;
    fg_stream << setprecision(prec);

    // Failure line.
    if (!fg_result->wasSuccessful()) {
        fg_stream << endl
                  << "Failures = " << fg_result->testFailures() << endl;
        fg_stream << "Errors   = " << fg_result->testErrors() << endl;
    }
    writeJunit();
}


// Print a summary after a successful run of the test suite.
void fgJunitOutputter::printSuccess()
{
    // Final summary.
    fg_stream << endl
              << "[ OK ]" << endl
              << endl;
    fg_stream.flush();
    writeJunit();
}

void fgJunitOutputter::writeJunit()
{
    junitReportFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    junitReportFile << "<testsuites>" << endl;
    junitReportFile << "<testsuite>" << endl;
    // A summary with timing info.
    auto localType = type;
    sg_ofstream& localjunitReportFile = junitReportFile;
    std::vector<TestDataCapt>* localtest_data_records = test_data_records;

    std::vector<std::string> testclasses;
    std::for_each(test_data_records->begin(), test_data_records->end(),
                  [&testclasses](TestDataCapt test_data) {
                      testclasses.push_back(test_data.name.substr(0, test_data.name.find("::")));
                  });
    auto last = std::unique(testclasses.begin(), testclasses.end());
    testclasses.erase(last, testclasses.end());
    std::sort(testclasses.begin(), testclasses.end());

    std::for_each(testclasses.begin(), testclasses.end(),
                  [&localType, &localtest_data_records, &localjunitReportFile](string testClass) {
                      time_t classTiming = 0;
                      std::for_each(localtest_data_records->begin(), localtest_data_records->end(),
                                    [&localjunitReportFile, &classTiming, &testClass](TestDataCapt test_data) {
                                        if (test_data.name.rfind(testClass, 0) == 0) {
                                            classTiming += test_data.timing;
                                        }
                                    });
                      std::for_each(localtest_data_records->begin(), localtest_data_records->end(),
                                    [&localType, &localjunitReportFile, &classTiming, &testClass](TestDataCapt test_data) {
                                        if (test_data.name.rfind(testClass, 0) == 0) {
                                            auto methodName = test_data.name.substr(test_data.name.find("::") + 2, test_data.name.length());
                                            localjunitReportFile << "<testcase suite_name=\"" << localType << "\" classname=\"" << testClass << "\" name=\"" << methodName << "\" time=\"" << ((double)test_data.timing / 1000000) << "\" file=\""
                                                                 << test_data.fileName << "\">" << endl;
                                            if (test_data.failure) {
                                                localjunitReportFile << "<failure>" << endl;
                                                localjunitReportFile << fgJunitOutputter::xmlEscape(test_data.failureText) << endl;
                                                localjunitReportFile << "</failure>" << endl;
                                                localjunitReportFile << "<system-out>" << endl;
                                                localjunitReportFile << fgJunitOutputter::xmlEscape(test_data.sg_interleaved) << endl;
                                                localjunitReportFile << "</system-out>" << endl;
                                            }
                                            localjunitReportFile << "</testcase>" << endl;
                                        }
                                    });
                  });

    localjunitReportFile << "</testsuite>" << endl;

    junitReportFile << "</testsuites>";
    junitReportFile.close();
}

std::string fgJunitOutputter::xmlEscape(const std::string& input)
{
    std::string out;
    for (std::string::const_iterator it = input.cbegin(); it != input.cend(); ++it) {
        switch (*it) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '\"': out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default: out += (*it); break;
        }
    }
    return out;
}
