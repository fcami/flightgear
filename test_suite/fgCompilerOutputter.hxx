
/*
 * SPDX-FileCopyrightText: (C) 2016 Edward d'Auvergne
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <cppunit/CompilerOutputter.h>
#include <cppunit/TestFailure.h>

#include "fgTestListener.hxx"


// The custom outputter for the FlightGear test suite.
class fgCompilerOutputter : public CppUnit::CompilerOutputter
{
    public:
        // Constructor.
        fgCompilerOutputter(CppUnit::TestResultCollector *result,
            std::vector<TestDataCapt> *capt,
            const clock_t *clock,
            CppUnit::OStream &stream,
            bool ctest = false,
            bool debug = false,
            const std::string &locationFormat = CPPUNIT_COMPILER_LOCATION_FORMAT)
        : CppUnit::CompilerOutputter(result, stream, locationFormat)
        , test_data_records(capt)
        , fg_result(result)
        , fg_stream(stream)
        , suite_timer(clock)
        , ctest_output(ctest)
        , debug(debug)
        {
        }

        // Create a new class instance.
        static fgCompilerOutputter *defaultOutputter(CppUnit::TestResultCollector *result, std::vector<TestDataCapt> *capt, const clock_t *clock, CppUnit::OStream &stream);

        // Print a summary after a successful run of the test suite.
        void printSuccess();

        // Detailed printout after a failed run of the test suite.
        void printFailureReport();

        // Printout for each failed test.
        void printFailureDetail(CppUnit::TestFailure *failure);

        // Printout of the test suite stats.
        void printSuiteStats();

        // The captured IO for each failed test.
        std::vector<TestDataCapt> *test_data_records;

    private:
        // Store copies of the base class objects.
        CppUnit::TestResultCollector *fg_result;
        CppUnit::OStream &fg_stream;

        // The test suite time, in clock ticks.
        const clock_t *suite_timer;

        // Output control.
        bool ctest_output;
        bool debug;

        // Simgear logstream IO printout.
        void printIOStreamMessages(std::string heading, std::string messages, bool empty);
        void printIOStreamMessages(std::string heading, std::string messages) {printIOStreamMessages(heading, messages, false);}
};
