// SPDX-FileCopyrightText: (C) 2025  James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "test_history.hxx"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>

#include "cppunit/TestAssert.h"
#include "simgear/structure/exception.hxx"
#include "test_suite/FGTestApi/NavDataCache.hxx"
#include "test_suite/FGTestApi/TestPilot.hxx"
#include "test_suite/FGTestApi/testGlobals.hxx"

#include <Airports/airport.hxx>
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>

using namespace std::chrono_literals;

HistoryTests::~HistoryTests() = default;

// Set up function for each test.
void HistoryTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("FlightHistory");
    FGTestApi::setUp::initNavDataCache();

    fgSetBool("/sim/history/enabled", true);
    history.reset(new FGFlightHistory);

    globals->get_subsystem_mgr()->add("history", history);

    globals->get_subsystem_mgr()->bind();
    globals->get_subsystem_mgr()->init();
}

// Clean up after each test.
void HistoryTests::tearDown()
{
    history->unbind();
    FGTestApi::tearDown::shutdownTestGlobals();
}

void HistoryTests::testClear()
{
    auto lfpg = FGAirport::getByIdent("LFPG");
    auto pilot = SGSharedPtr<FGTestApi::TestPilot>(new FGTestApi::TestPilot);
    FGTestApi::setPosition(lfpg->geod());
    pilot->resetAtPosition(lfpg->geod());

    pilot->setSpeedKts(120);
    pilot->setCourseTrue(180.0);
    pilot->setTargetAltitudeFtMSL(10000);

    FGTestApi::runForTime(180);

    auto path = history->pathForHistory();
    CPPUNIT_ASSERT_EQUAL(size_t{174}, path.size());

    auto pp = history->pagedPathForHistory(100);
    CPPUNIT_ASSERT_EQUAL(size_t{100}, pp->path.size());

    // clear completely
    history->clear();

    auto path2 = history->pathForHistory();
    CPPUNIT_ASSERT(path2.empty());

    auto pp2 = history->pagedPathForHistory(100);
    CPPUNIT_ASSERT(pp2->path.empty());


    FGTestApi::runForTime(3000);
    // partial clear
    history->clearOlderThan(1500s);

    auto path4 = history->pathForHistory();
    CPPUNIT_ASSERT_EQUAL(size_t{1888}, path4.size());
}
