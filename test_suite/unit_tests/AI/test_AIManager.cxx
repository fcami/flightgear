/*
 * Copyright (C) 2020 James Turner
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This file is part of the program FlightGear.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "test_AIManager.hxx"

#include <cstring>
#include <memory>

#include "test_suite/FGTestApi/NavDataCache.hxx"
#include "test_suite/FGTestApi/TestDataLogger.hxx"
#include "test_suite/FGTestApi/TestPilot.hxx"
#include "test_suite/FGTestApi/testGlobals.hxx"

#include <AIModel/AIAircraft.hxx>
#include <AIModel/AIFlightPlan.hxx>
#include <AIModel/AIManager.hxx>

#include <Airports/airport.hxx>
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Navaids/NavDataCache.hxx>
#include <Navaids/navrecord.hxx>

#include <simgear/misc/sg_path.hxx>
#include <simgear/structure/commands.hxx>

/////////////////////////////////////////////////////////////////////////////

// Set up function for each test.
void AIManagerTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("AI");
    FGTestApi::setUp::initNavDataCache();

    globals->get_subsystem_mgr()->add<FGAIManager>();

    auto props = globals->get_props();
    props->setBoolValue("sim/ai/enabled", true);

    globals->get_subsystem_mgr()->bind();
    globals->get_subsystem_mgr()->init();
    globals->get_subsystem_mgr()->postinit();
}

// Clean up after each test.
void AIManagerTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}

void AIManagerTests::testBasic()
{
    auto aim = globals->get_subsystem<FGAIManager>();

    auto bikf = FGAirport::findByIdent("BIKF");

    auto pilot = SGSharedPtr<FGTestApi::TestPilot>(new FGTestApi::TestPilot);
    FGTestApi::setPosition(bikf->geod());
    pilot->resetAtPosition(bikf->geod());


    pilot->setSpeedKts(220);
    pilot->setCourseTrue(0.0);
    pilot->setTargetAltitudeFtMSL(10000);

    FGTestApi::runForTime(10.0);

    auto aiUserAircraft = aim->getUserAircraft();
    CPPUNIT_ASSERT(aiUserAircraft->isValid());
    CPPUNIT_ASSERT(!aiUserAircraft->getDie());

    const SGGeod g = globals->get_aircraft_position();
    CPPUNIT_ASSERT_DOUBLES_EQUAL(g.getLongitudeDeg(), aiUserAircraft->getGeodPos().getLongitudeDeg(), 0.01);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(g.getLatitudeDeg(), aiUserAircraft->getGeodPos().getLatitudeDeg(), 0.01);

    // disable, the AI user aircraft doesn't track altitude?!
    //   CPPUNIT_ASSERT_DOUBLES_EQUAL(g.getElevationFt(), aiUserAircraft->getGeodPos().getElevationFt(), 1);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(fgGetDouble("orientation/heading-deg"), aiUserAircraft->getTrueHeadingDeg(), 1);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(fgGetDouble("velocities/groundspeed-kt"), aiUserAircraft->getSpeed(), 1);
}

// test for AIFLightPlan leg / legEnd pieces.

void AIManagerTests::testAircraftWaypoints()
{
    auto aim = globals->get_subsystem<FGAIManager>();

    SGPropertyNode_ptr aircraftDefinition(new SGPropertyNode);
    aircraftDefinition->setStringValue("type", "aircraft");
    aircraftDefinition->setStringValue("callsign", "G-ARTA");
    // set class for performance data


    auto eggd = FGAirport::findByIdent("EGGD");
    aircraftDefinition->setDoubleValue("heading", 90.0);
    aircraftDefinition->setDoubleValue("latitude", eggd->geod().getLatitudeDeg());
    aircraftDefinition->setDoubleValue("longitude", eggd->geod().getLongitudeDeg());
    aircraftDefinition->setDoubleValue("altitude", 6000.0);
    aircraftDefinition->setDoubleValue("speed", 250.0); // IAS or TAS?

    FGTestApi::setPositionAndStabilise(eggd->geod());

    auto ai = aim->addObject(aircraftDefinition);
    CPPUNIT_ASSERT(ai);
    CPPUNIT_ASSERT_EQUAL(FGAIBase::object_type::otAircraft, ai->getType());
    CPPUNIT_ASSERT_EQUAL(std::string{"aircraft"}, std::string{ai->getTypeString()});

    auto aiAircraft = static_cast<FGAIAircraft*>(ai.get());

    const auto aiPos = aiAircraft->getGeodPos();
    CPPUNIT_ASSERT_DOUBLES_EQUAL(eggd->geod().getLatitudeDeg(), aiPos.getLatitudeDeg(), 0.01);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(eggd->geod().getLongitudeDeg(), aiPos.getLongitudeDeg(), 0.01);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(90.0, aiAircraft->getTrueHeadingDeg(), 1);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(250.0, aiAircraft->getSpeed(), 1);

    std::unique_ptr<FGAIFlightPlan> aiFP(new FGAIFlightPlan);
    ai->setFlightPlan(std::move(aiFP));
}

// A loaded scenario (e.g. a carrier) must survive a subsystem reinit.
// postinit() reloads scenarios from the /sim/ai/scenario list, so reinit() must
// preserve that list; if it clears it, the scenario - and any carrier it placed
// - vanishes on a reset/reposition. This checks both the list and the resulting
// AI objects survive a reinit. See issue #3388.
void AIManagerTests::testReinitPreservesScenario()
{
    auto aim = globals->get_subsystem<FGAIManager>();

    // Register a minimal scenario file into the catalog at /sim/ai/scenarios,
    // exactly as registerScenarios() does for files found in $FG_ROOT/AI.
    const SGPath scenarioPath = SGPath::fromUtf8(FG_TEST_SUITE_DATA) / "AI" / "test_scenario.xml";
    CPPUNIT_ASSERT(scenarioPath.exists());
    FGAIManager::registerScenarioFile(globals->get_props(), scenarioPath);

    // Load it through the load-scenario command. This both creates the scenario
    // objects and records the scenario in the active list at /sim/ai/scenario -
    // the same path a --ai-scenario= start or the GUI dialog would take.
    SGPropertyNode_ptr args(new SGPropertyNode);
    args->setStringValue("name", "test_scenario");
    CPPUNIT_ASSERT(globals->get_commands()->execute("load-scenario", args));

    SGPropertyNode* scenarioRoot = globals->get_props()->getNode("sim/ai");
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), scenarioRoot->getChildren("scenario").size());

    // The scenario's ship object should now be attached.
    CPPUNIT_ASSERT(!aim->get_ai_list().empty());

    // Reinit the subsystem - the operation that issue #3388 regressed on.
    aim->reinit();

    // The active scenario list must survive the reinit so that postinit() can
    // reload it...
    const auto activeAfter = scenarioRoot->getChildren("scenario");
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), activeAfter.size());
    CPPUNIT_ASSERT_EQUAL(std::string{"test_scenario"},
                         std::string{activeAfter.front()->getStringValue()});

    // ...and the scenario objects must be present again rather than gone.
    CPPUNIT_ASSERT(!aim->get_ai_list().empty());
}
