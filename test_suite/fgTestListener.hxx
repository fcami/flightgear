
/*
 * SPDX-FileCopyrightText: (C) 2016 Edward d'Auvergne
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <time.h>
#include <vector>

#include "formatting.hxx"


// Data structure for holding the captured data for a test.
struct TestDataCapt {
    std::string name;
    time_t timing;
    bool failure;
    bool error;
    std::string fileName;
    std::string failureText;
    std::string stdio;
    std::string sg_interleaved;
    std::string sg_bulk_only;
    std::string sg_debug_only;
    std::string sg_info_only;
    std::string sg_warn_only;
    std::string sg_alert_only;
};


// Match the test by name for std:find using a vector<TestDataCapt>.
class matchTestName
{
    std::string _name;

public:
    matchTestName(const std::string& name) : _name(name) {}

    bool operator()(const TestDataCapt& item) const
    {
        return item.name == _name;
    }
};


// The custom test runner for the FlightGear test suite.
class fgTestListener : public CppUnit::TestListener
{
protected:
    // Failure state.
    bool m_failure, m_error;

public:
    // Constructor.
    fgTestListener() : m_failure(false), m_error(false), sum_time(0){};

    // Override the base class function to capture IO. streams
    void startTest(CppUnit::Test* test);

    // Override the base class function to restore IO streams.
    void endTest(CppUnit::Test* test);

    // Handle failures.
    void addFailure(const CppUnit::TestFailure& failure);

    // Test suite timing.
    clock_t sum_time;

    // IO capture for all failed tests.
    std::vector<TestDataCapt> test_data_records;

    // Output settings.
    bool timings;
    bool ctest_output;
    bool junit_output;
    bool debug;

protected:
    // The original IO streams.
    std::streambuf *orig_cerr, *orig_cout;

    // Captured IO streams.
    std::stringstream capt;

    // Test timings.
    clock_t m_time;
};
