// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "test_procedures.hxx"

#include <simgear/math/SGMath.hxx>
#include <simgear/misc/strutils.hxx>

#include "test_suite/FGTestApi/NavDataCache.hxx"
#include "test_suite/FGTestApi/testGlobals.hxx"

#include <Airports/airport.hxx>
#include <Navaids/navrecord.hxx>
#include <Navaids/procedure.hxx>
#include <Navaids/waypoint.hxx>

using namespace flightgear;
using namespace std::string_literals;

namespace {

string_list identsOf(const WayptVec& wps)
{
    string_list r;
    for (const auto& w : wps) {
        r.push_back(w->ident());
    }
    return r;
}

WayptVec routeSTAR(const std::string& star, const std::string& transition)
{
    auto eghi = FGAirport::findByIdent("EGHI"s);
    auto s = eghi->findSTARWithIdent(star);
    CPPUNIT_ASSERT(s);
    Transition* t = nullptr;
    if (!transition.empty()) {
        t = s->findTransitionByName(transition);
        CPPUNIT_ASSERT(t);
    }

    WayptVec path;
    CPPUNIT_ASSERT(s->route(nullptr, t, path));
    return path;
}

WayptVec routeSID(const std::string& sid, const std::string& transition)
{
    auto eghi = FGAirport::findByIdent("EGHI"s);
    auto s = eghi->findSIDWithIdent(sid);
    CPPUNIT_ASSERT(s);
    Transition* t = nullptr;
    if (!transition.empty()) {
        t = s->findTransitionByName(transition);
        CPPUNIT_ASSERT(t);
    }

    WayptVec path;
    CPPUNIT_ASSERT(s->route(eghi->getRunwayByIdent("20"s), t, path));
    return path;
}

FGNavRecordRef southamptonVOR()
{
    FGPositioned::TypeFilter f{FGPositioned::VOR};
    auto eghi = FGAirport::findByIdent("EGHI"s);
    return fgpositioned_cast<FGNavRecord>(FGPositioned::findClosestWithIdent("SAM"s, eghi->geod(), &f));
}

} // namespace

// Set up function for each test.
void ProcedureTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("procedures"s);
    FGTestApi::setUp::initNavDataCache();
}

// Clean up after each test.
void ProcedureTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}

void ProcedureTests::testTransitionEndingOnCommonFix()
{
    // the shared fix must appear once
    CPPUNIT_ASSERT_EQUAL("HIT01 HIT02 HIA01 HIA02 HIA03"s, simgear::strutils::join(identsOf(routeSTAR("JOIN1A"s, "HIT01"s)), " "));
}

void ProcedureTests::testTransitionJoiningPartway()
{
    // common fixes before the join point are skipped
    CPPUNIT_ASSERT_EQUAL("HIT03 HIA02 HIA03"s, simgear::strutils::join(identsOf(routeSTAR("JOIN1A"s, "HIT03"s)), " "));
}

void ProcedureTests::testTransitionNotJoining()
{
    CPPUNIT_ASSERT_EQUAL("HIT04 HIT05 HIA01 HIA02 HIA03"s, simgear::strutils::join(identsOf(routeSTAR("JOIN1A"s, "HIT04"s)), " "));
    CPPUNIT_ASSERT_EQUAL("HIA01 HIA02 HIA03"s, simgear::strutils::join(identsOf(routeSTAR("JOIN1A"s, ""s)), " "));
}

void ProcedureTests::testSIDTransitionStartingOnCommonFix()
{
    CPPUNIT_ASSERT_EQUAL("HID01 HID02 HID03 HID04"s, simgear::strutils::join(identsOf(routeSID("JOIN2"s, "HID04"s)), " "));
    CPPUNIT_ASSERT_EQUAL("HID01 HID02"s, simgear::strutils::join(identsOf(routeSID("JOIN2"s, ""s)), " "));
}

void ProcedureTests::testWaypointsDontInheritRestrictions()
{
    auto path = routeSTAR("LEAK1A"s, "HIT06"s);
    CPPUNIT_ASSERT_EQUAL("HIT06 HIT07 HIA04 HIA05"s, simgear::strutils::join(identsOf(path), " "));

    // fully-specified waypoints, in the transition and the common route
    auto hit06 = path.at(0);
    CPPUNIT_ASSERT_EQUAL(RESTRICT_AT, hit06->speedRestriction());
    CPPUNIT_ASSERT_DOUBLES_EQUAL(250.0, hit06->speedKts(), 0.1);
    CPPUNIT_ASSERT_EQUAL(RESTRICT_ABOVE, hit06->altitudeRestriction());
    CPPUNIT_ASSERT_DOUBLES_EQUAL(9000.0, hit06->altitudeFt(), 0.1);
    CPPUNIT_ASSERT(hit06->flag(WPT_OVERFLIGHT));

    auto hia04 = path.at(2);
    CPPUNIT_ASSERT_EQUAL(RESTRICT_AT, hia04->speedRestriction());
    CPPUNIT_ASSERT_EQUAL(RESTRICT_AT, hia04->altitudeRestriction());
    CPPUNIT_ASSERT(hia04->flag(WPT_OVERFLIGHT));

    // the following waypoints state nothing, so must have no restrictions
    for (int i : {1, 3}) {
        auto w = path.at(i);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(w->ident(), RESTRICT_NONE, w->speedRestriction());
        CPPUNIT_ASSERT_EQUAL_MESSAGE(w->ident(), RESTRICT_NONE, w->altitudeRestriction());
        CPPUNIT_ASSERT_MESSAGE(w->ident(), !w->flag(WPT_OVERFLIGHT));
    }
}

void ProcedureTests::testHoldDefaults()
{
    auto path = routeSTAR("HOLD1A"s, ""s);
    CPPUNIT_ASSERT_EQUAL("HIA06 HIA06 HIA07"s, simgear::strutils::join(identsOf(path), " "));

    auto first = dynamic_cast<Hold*>(path.at(1).get());
    CPPUNIT_ASSERT(first);
    CPPUNIT_ASSERT(first->isLeftHanded());
    CPPUNIT_ASSERT(first->isDistance());
    CPPUNIT_ASSERT_DOUBLES_EQUAL(5.0, first->timeOrDistance(), 0.01);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(130.0, first->inboundRadial(), 0.01);

    // specifies nothing: standard right-hand hold, one minute legs
    auto second = dynamic_cast<Hold*>(path.at(2).get());
    CPPUNIT_ASSERT(second);
    CPPUNIT_ASSERT(!second->isLeftHanded());
    CPPUNIT_ASSERT(!second->isDistance());
    CPPUNIT_ASSERT_DOUBLES_EQUAL(60.0, second->timeOrDistance(), 0.01);
}

void ProcedureTests::testPBDLeg()
{
    auto path = routeSID("COVER1"s, ""s);
    auto pbd = path.at(1);
    CPPUNIT_ASSERT_EQUAL("SAM05"s, pbd->ident());
    CPPUNIT_ASSERT_EQUAL("basic"s, pbd->type());

    // 5nm on a bearing of 090 from the SAM VOR
    auto sam = southamptonVOR();
    CPPUNIT_ASSERT(sam);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(5.0, SGGeodesy::distanceNm(sam->geod(), pbd->position()), 0.05);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(90.0, SGGeodesy::courseDeg(sam->geod(), pbd->position()), 0.5);
}

void ProcedureTests::testInterceptAndVectorLegs()
{
    auto path = routeSID("COVER1"s, ""s);
    CPPUNIT_ASSERT_EQUAL("(1000) SAM05 SAM12 SAMR VECTORS-EGHI"s, simgear::strutils::join(identsOf(path), " "));

    auto hdg = dynamic_cast<HeadingToAltitude*>(path.at(0).get());
    CPPUNIT_ASSERT(hdg);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(200.0, hdg->headingDegMagnetic(), 0.01);
    CPPUNIT_ASSERT_EQUAL(RESTRICT_ABOVE, hdg->altitudeRestriction());
    CPPUNIT_ASSERT_DOUBLES_EQUAL(1000.0, hdg->altitudeFt(), 0.1);

    auto dme = dynamic_cast<DMEIntercept*>(path.at(2).get());
    CPPUNIT_ASSERT(dme);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(200.0, dme->courseDegMagnetic(), 0.01);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(12.0, dme->dmeDistanceNm(), 0.01);

    auto radial = dynamic_cast<RadialIntercept*>(path.at(3).get());
    CPPUNIT_ASSERT(radial);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(270.0, radial->courseDegMagnetic(), 0.01);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(180.0, radial->radialDegMagnetic(), 0.01);

    CPPUNIT_ASSERT_EQUAL("vectors"s, path.at(4)->type());
}
