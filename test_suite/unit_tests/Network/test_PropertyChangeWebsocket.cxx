// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "test_PropertyChangeWebsocket.hxx"
#include "httpTestHelpers.hxx"

#include "test_suite/FGTestApi/testGlobals.hxx"

#include <Main/fg_props.hxx>
#include <Network/http/PropertyChangeObserver.hxx>
#include <Network/http/PropertyChangeWebsocket.hxx>

#include <nlohmann/json.hpp>

using namespace flightgear::http;
using namespace FGTestApi::http;

void PropertyChangeWebsocketTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("PropertyChangeWebsocket");
    // Disable the per-poll rate throttle so every poll() fires unconditionally.
    // This value is read by the PropertyChangeWebsocket constructor.
    fgSetDouble("/sim/http/property-websocket/update-interval-secs", 0.0);
}

void PropertyChangeWebsocketTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}

// ---------------------------------------------------------------------------

void PropertyChangeWebsocketTests::testAddListenerAndPoll()
{
    fgSetString("/test/pcws/value", "hello");

    PropertyChangeObserver observer;
    PropertyChangeWebsocket ws(&observer);
    MockWebsocketWriter writer;

    // addListener registers the observation; a freshly-watched node's own
    // "last seen version" starts at 0 while the observer entry's version
    // starts at 1, so the very first poll() will emit the current value
    // without needing check() first
    auto req = buildRequest("GET", "/PropertyListener",
                            R"({"command":"addListener","node":"/test/pcws/value"})");
    ws.handleRequest(req, writer);
    ws.poll(writer);

    CPPUNIT_ASSERT(!writer.frames.empty());
    auto json = nlohmann::json::parse(writer.frames.back());
    CPPUNIT_ASSERT_EQUAL(std::string("/test/pcws/value"), json.at("path").get<std::string>());
    CPPUNIT_ASSERT_EQUAL(std::string("hello"), json.at("value").get<std::string>());
}

void PropertyChangeWebsocketTests::testGetCommand()
{
    fgSetDouble("/test/pcws/altitude", 5000.0);

    PropertyChangeObserver observer;
    PropertyChangeWebsocket ws(&observer);
    MockWebsocketWriter writer;

    // get returns the value immediately, without registering a listener
    auto req = buildRequest("GET", "/PropertyListener",
                            R"({"command":"get","node":"/test/pcws/altitude"})");
    ws.handleRequest(req, writer);

    CPPUNIT_ASSERT_EQUAL(size_t(1), writer.frames.size());
    auto json = nlohmann::json::parse(writer.frames.front());
    CPPUNIT_ASSERT_DOUBLES_EQUAL(5000.0, json.at("value").get<double>(), 1.0);
}

void PropertyChangeWebsocketTests::testSetCommand()
{
    fgSetDouble("/test/pcws/heading", 90.0);

    PropertyChangeObserver observer;
    PropertyChangeWebsocket ws(&observer);
    MockWebsocketWriter writer;

    auto req = buildRequest("GET", "/PropertyListener",
                            R"({"command":"set","node":"/test/pcws/heading","value":180.0})");
    ws.handleRequest(req, writer);

    // set writes to the property tree; no frame is emitted
    CPPUNIT_ASSERT(writer.frames.empty());
    CPPUNIT_ASSERT_DOUBLES_EQUAL(180.0, fgGetDouble("/test/pcws/heading"), 0.01);
}

void PropertyChangeWebsocketTests::testNoChangePollEmitsNothing()
{
    fgSetString("/test/pcws/stable", "constant");

    PropertyChangeObserver observer;
    PropertyChangeWebsocket ws(&observer);
    MockWebsocketWriter writer;

    auto req = buildRequest("GET", "/PropertyListener",
                            R"({"command":"addListener","node":"/test/pcws/stable"})");
    ws.handleRequest(req, writer);

    // The observer's _prevValue starts empty, so the first check() call
    // sees a change (empty -> "constant") and the following poll() emits
    // it once (change detection is a per-entry version counter, not a
    // shared consume-once flag -- see PropertyChangeObserver.hxx). A
    // second cycle confirms it stays quiet once stable.
    for (int i = 0; i < 2; ++i) {
        observer.check();
        ws.poll(writer);
    }

    // Third cycle: value is unchanged → poll must be silent
    observer.check();
    writer.frames.clear();
    ws.poll(writer);

    CPPUNIT_ASSERT(writer.frames.empty());
}
