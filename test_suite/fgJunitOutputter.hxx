/*
 * SPDX-FileName: fgJunitOutputter.hxx
 * SPDX-FileComment: Logger for JUnit
 * SPDX-FileCopyrightText: Copyright (C) 2016 Edward d'Auvergne, 2025 Keith Paterson
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <algorithm>

#include <cppunit/CompilerOutputter.h>
#include <cppunit/TestFailure.h>

#include <simgear/io/iostreams/sgstream.hxx>
#include <simgear/misc/sg_path.hxx>

#include <cppunit/Exception.h>

#include "fgTestListener.hxx"


// The custom outputter for the FlightGear test suite.
class fgJunitOutputter : public CppUnit::CompilerOutputter
{
public:
    // Constructor.
    fgJunitOutputter(const std::string& type,
                     CppUnit::TestResultCollector* result,
                     std::vector<TestDataCapt>* capt,
                     const clock_t* clock,
                     CppUnit::OStream& stream,
                     bool debug = false,
                     const std::string& locationFormat = CPPUNIT_COMPILER_LOCATION_FORMAT)
        : CppUnit::CompilerOutputter(result, stream, locationFormat), test_data_records(capt), type(type), fg_result(result), fg_stream(stream), suite_timer(clock), debug(debug)
    {
        std::string fName = type + "_junit.xml";
        std::replace(fName.begin(), fName.end(), ' ', '_');
        std::replace(fName.begin(), fName.end(), '/', '_');
        SGPath p(fName);
        junitReportFile.open(p);
    }

    // Create a new class instance.
    static fgJunitOutputter* defaultOutputter(const std::string&, CppUnit::TestResultCollector* result, std::vector<TestDataCapt>* capt, const clock_t* clock, CppUnit::OStream& stream);

    // Print a summary after a successful run of the test suite.
    void printSuccess();

    // Printout for each failed test.
    void printFailureDetail(CppUnit::TestFailure* failure);

    // Printout of the test suite stats.
    void printStatistics();

    // The captured data for each test.
    std::vector<TestDataCapt>* test_data_records;

private:
    std::string type;
    // Store copies of the base class objects.
    CppUnit::TestResultCollector* fg_result;
    CppUnit::OStream& fg_stream;

    // The test suite time, in clock ticks.
    const clock_t* suite_timer;

    // Output control.
    bool debug;

    sg_ofstream junitReportFile;

    void writeJunit();
    static std::string xmlEscape(const std::string& input);
    //        void writeJunitTestresult(const TestDataCapt test_data);
    // Simgear logstream IO printout.
    void printIOStreamMessages(std::string heading, std::string messages, bool empty);
    void printIOStreamMessages(std::string heading, std::string messages) { printIOStreamMessages(heading, messages, false); }

    std::string getMessage(CppUnit::Message m) const
    {
        return m.shortDescription() + '\n' +
               m.details();
    }
};
