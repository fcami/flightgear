// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "test_MirrorPropertyTreeWebsocket.hxx"
#include "httpTestHelpers.hxx"

#include "test_suite/FGTestApi/testGlobals.hxx"

#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Network/http/MirrorPropertyTreeWebsocket.hxx>

#include <simgear/props/props.hxx>

#include <nlohmann/json.hpp>

using namespace flightgear::http;
using namespace FGTestApi::http;

// Helper: drain the initial "created" frame emitted when a websocket first
// registers a pre-existing sub-tree, then clear the frame list.
static void drainInitialFrame(MirrorPropertyTreeWebsocket& ws, MockWebsocketWriter& writer)
{
    ws.poll(writer);
    writer.frames.clear();
}

void MirrorPropertyTreeWebsocketTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("MirrorPropertyTreeWebsocket");
    // Disable the send-rate throttle so every poll() fires immediately.
    // This value is read by the MirrorPropertyTreeWebsocket constructor.
    fgSetInt("/sim/http/mirror-websocket/update-interval-ms", 0);

    // Production code sets VALUE_CHANGED_DOWN on /sim so that value changes
    // on any descendant propagate up to ancestor listeners.  Replicate that
    // for the /test namespace used by these tests.
    fgGetNode("/test", true)->setAttribute(SGPropertyNode::VALUE_CHANGED_DOWN, true);
}

void MirrorPropertyTreeWebsocketTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}

// ---------------------------------------------------------------------------

void MirrorPropertyTreeWebsocketTests::testInitialSubtreeSent()
{
    // Build the sub-tree before constructing the websocket so the listener
    // picks it up via registerSubtree() in checkNodeExists().
    fgSetString("/test/mirror/name", "FlightGear");
    fgSetDouble("/test/mirror/altitude", 10000.0);

    MirrorPropertyTreeWebsocket ws("/test/mirror");
    MockWebsocketWriter writer;

    ws.poll(writer);

    CPPUNIT_ASSERT_EQUAL(size_t(1), writer.frames.size());
    auto json = nlohmann::json::parse(writer.frames.front());

    // The initial frame must contain a "created" array with our nodes
    CPPUNIT_ASSERT(json.contains("created"));
    CPPUNIT_ASSERT(!json.at("created").empty());
}

void MirrorPropertyTreeWebsocketTests::testNewChildSent()
{
    fgSetString("/test/mirror2/existing", "here");

    MirrorPropertyTreeWebsocket ws("/test/mirror2");
    MockWebsocketWriter writer;
    drainInitialFrame(ws, writer);

    // Add a new child after the websocket is already connected
    fgSetDouble("/test/mirror2/newchild", 42.0);
    ws.poll(writer);

    CPPUNIT_ASSERT_EQUAL(size_t(1), writer.frames.size());
    auto json = nlohmann::json::parse(writer.frames.front());

    CPPUNIT_ASSERT(json.contains("created"));
    const auto& created = json.at("created");
    bool found = false;
    for (const auto& entry : created) {
        if (entry.value("path", std::string{}) == "/test/mirror2/newchild") {
            found = true;
            break;
        }
    }
    CPPUNIT_ASSERT_MESSAGE("new child not found in 'created'", found);
}

void MirrorPropertyTreeWebsocketTests::testValueChangeSent()
{
    fgSetDouble("/test/mirror3/speed", 100.0);

    MirrorPropertyTreeWebsocket ws("/test/mirror3");
    MockWebsocketWriter writer;
    drainInitialFrame(ws, writer);

    // Change the value; the listener will pick this up synchronously
    fgSetDouble("/test/mirror3/speed", 250.0);
    ws.poll(writer);

    CPPUNIT_ASSERT_EQUAL(size_t(1), writer.frames.size());
    auto json = nlohmann::json::parse(writer.frames.front());

    // A "changed" array should be present — it contains [id, newValue] pairs
    CPPUNIT_ASSERT(json.contains("changed"));
    CPPUNIT_ASSERT(!json.at("changed").empty());
}

void MirrorPropertyTreeWebsocketTests::testRemovedChildSent()
{
    fgSetString("/test/mirror4/child", "toremove");

    MirrorPropertyTreeWebsocket ws("/test/mirror4");
    MockWebsocketWriter writer;
    drainInitialFrame(ws, writer);

    // Remove the child node
    SGPropertyNode_ptr root = globals->get_props()->getNode("/test/mirror4");
    CPPUNIT_ASSERT(root != nullptr);
    root->removeChild("child", 0);

    ws.poll(writer);

    CPPUNIT_ASSERT_EQUAL(size_t(1), writer.frames.size());
    auto json = nlohmann::json::parse(writer.frames.front());

    // A "removed" array of integer IDs should be present
    CPPUNIT_ASSERT(json.contains("removed"));
    CPPUNIT_ASSERT(!json.at("removed").empty());
}

void MirrorPropertyTreeWebsocketTests::testDeleteAndReAddAppearsAsChange()
{
    // This simulates the Nasal pattern where a property is deleted and
    // immediately re-created (e.g. by a Nasal timer) between two polls.
    // The listener keeps a recentlyRemoved list so it can recognise the
    // re-add as a recycled node and emit only a "changed" entry rather
    // than a "removed" + "created" pair.
    fgSetString("/test/mirror5/item", "original");

    MirrorPropertyTreeWebsocket ws("/test/mirror5");
    MockWebsocketWriter writer;
    drainInitialFrame(ws, writer);

    // Remove then immediately re-create the child, all before the next poll.
    SGPropertyNode_ptr root = globals->get_props()->getNode("/test/mirror5");
    CPPUNIT_ASSERT(root != nullptr);
    root->removeChild("item", 0);
    fgSetString("/test/mirror5/item", "updated"); // same path, new value

    ws.poll(writer);

    CPPUNIT_ASSERT_EQUAL(size_t(1), writer.frames.size());
    auto json = nlohmann::json::parse(writer.frames.front());

    // The recycled node must appear as a value change, not a remove/create pair.
    CPPUNIT_ASSERT_MESSAGE("unexpected 'removed' entry", !json.contains("removed"));
    CPPUNIT_ASSERT_MESSAGE("unexpected 'created' entry", !json.contains("created"));
    CPPUNIT_ASSERT(json.contains("changed"));
    CPPUNIT_ASSERT(!json.at("changed").empty());
}
