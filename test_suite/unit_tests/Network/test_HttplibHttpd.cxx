// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "test_HttplibHttpd.hxx"

#include "test_suite/FGTestApi/NavDataCache.hxx"
#include "test_suite/FGTestApi/testGlobals.hxx"

#include <Aircraft/FlightHistory.hxx>
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Network/http/HttplibHttpd.hxx>

#include <simgear/misc/sg_dir.hxx>
#include <simgear/package/Root.hxx>
#include <simgear/props/props.hxx>
#include <simgear/structure/commands.hxx>
#include <simgear/structure/event_mgr.hxx>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <thread>

using nlohmann::json;

namespace {

// A test-registered command for testRunHandlerGet -- RunUriHandler executes
// commands by name via SGCommandMgr, which takes a plain function pointer
// rather than std::function, so this can't be a capturing lambda.
bool testNoopCommand(const SGPropertyNode*, SGPropertyNode*)
{
    return true;
}

// A test-registered command for the POST/PUT/PATCH/DELETE body tests: copies
// its arg tree into the property tree so the test can assert on what
// RunUriHandler actually parsed out of the request body, rather than just
// whether the command executed at all - an empty arg tree still returns true
// here (see testNoopCommand), so checking only the response status wouldn't
// distinguish a correctly-parsed body from a silently empty one.
bool testEchoArgsCommand(const SGPropertyNode* arg, SGPropertyNode*)
{
    fgSetString("/test/http/run-echo/marker", arg->getStringValue("marker", "MISSING"));
    fgSetInt("/test/http/run-echo/number", arg->getIntValue("number", -1));
    return true;
}

} // namespace

void HttplibHttpdTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("HttplibHttpd");
    // NavdbUriHandler goes through FGAirport::findByIdent(), which needs a
    // real NavDataCache singleton -- without this it segfaults on a null
    // NavDataCachePrivate rather than returning "not found".
    FGTestApi::setUp::initNavDataCache();

    // FlightHistoryUriHandler dereferences this subsystem with no null
    // be registered before any request reaches /flighthistory/.
    auto history = globals->get_subsystem_mgr()->add<FGFlightHistory>();
    history->init();

    globals->get_commands()->addCommand("http-test-noop", &testNoopCommand);
    globals->get_commands()->addCommand("http-test-echo-args", &testEchoArgsCommand);

    _docRoot = SGPath(fgGetString("/sim/fg-home")) / "httpd-docroot";
    // SGPath::create_dir() creates this path's *parent* (it's meant for
    // "ensure the directory to hold this file exists"), not this
    // directory itself -- use simgear::Dir to actually create _docRoot.
    simgear::Dir(_docRoot).create(0755);
    {
        std::ofstream f((_docRoot / "hello.txt").utf8Str());
        f << "hello from the static file handler";
    }

    SGPropertyNode_ptr configNode = new SGPropertyNode;
    configNode->getNode("uri-handler/json", true)->setStringValue("/json/");
    configNode->getNode("uri-handler/property", true)->setStringValue("/props/");
    configNode->getNode("uri-handler/pkg", true)->setStringValue("/pkg/");
    configNode->getNode("uri-handler/run", true)->setStringValue("/run");
    configNode->getNode("uri-handler/navdb", true)->setStringValue("/navdb/");
    configNode->getNode("uri-handler/flighthistory", true)->setStringValue("/flighthistory/");

    // A fixed, high, unlikely-to-collide port -- see waitUntilServerIsUp()
    // for how the test synchronizes against the server actually being up
    // rather than assuming this succeeds instantly.
    _port = 18173;
    configNode->getNode("options/document-root", true)->setStringValue(_docRoot.utf8Str());
    configNode->getNode("options/listening-port", true)->setStringValue(std::to_string(_port));
    configNode->getNode("options/idle-timeout-ms", true)->setStringValue("2000");

    _httpd = std::make_unique<flightgear::HttplibHttpd>(configNode);
    _httpd->bind();
    waitUntilServerIsUp();

    // This runs on its own thread rather than the thread that called
    // initTestGlobals() (which is what a real app's single main thread
    // would be), so testRunHandlerGet still logs SGCommandMgr::execute()'s
    // "different thread than expected" debug warning even though the
    // main-thread hop itself works (the request succeeds, and does not
    // deadlock). In the real app, the event manager is always updated from
    // the same main thread that constructs SGCommandMgr, so that warning
    // doesn't fire there.
    _stopUpdatePump = false;
    _updatePumpThread = std::thread([this] {
        while (!_stopUpdatePump.load()) {
            globals->get_event_mgr()->update(0.005);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
}

void HttplibHttpdTests::tearDown()
{
    _stopUpdatePump = true;
    _updatePumpThread.join();

    _httpd->unbind();
    _httpd.reset();
    simgear::Dir(_docRoot).remove(true);

    FGTestApi::tearDown::shutdownTestGlobals();
}

void HttplibHttpdTests::waitUntilServerIsUp()
{
    httplib::Client client("127.0.0.1", _port);
    client.set_connection_timeout(0, 50000); // 50ms
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (auto res = client.Get("/hello.txt")) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    CPPUNIT_FAIL("HttplibHttpd never started accepting connections");
}

// ---------------------------------------------------------------------------

void HttplibHttpdTests::testStaticFile()
{
    httplib::Client client("127.0.0.1", _port);
    auto res = client.Get("/hello.txt");
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT_EQUAL(200, res->status);
    CPPUNIT_ASSERT_EQUAL(std::string("hello from the static file handler"), res->body);
}

void HttplibHttpdTests::testUnknownRouteFallsThroughToStaticFiles()
{
    // No URI handler is registered for /nonexistent/ at all, so this must
    // fall through to httplib's mounted static file serving (and 404, since
    // the file doesn't exist) -- rather than the server hanging or 500ing.
    httplib::Client client("127.0.0.1", _port);
    auto res = client.Get("/nonexistent/path");
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT_EQUAL(404, res->status);
}

void HttplibHttpdTests::testJsonHandler()
{
    fgSetString("/test/http/aircraft", "c172p");

    httplib::Client client("127.0.0.1", _port);
    auto res = client.Get("/json/test/http/aircraft");
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT_EQUAL(200, res->status);

    auto j = json::parse(res->body);
    CPPUNIT_ASSERT_EQUAL(std::string("c172p"), j.at("value").get<std::string>());
}

void HttplibHttpdTests::testPropertyHandler()
{
    httplib::Client client("127.0.0.1", _port);
    auto res = client.Get("/props/");
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT_EQUAL(200, res->status);
    // It's an HTML listing of the property root's children.
    CPPUNIT_ASSERT(res->body.find("<html") != std::string::npos || res->body.find("<HTML") != std::string::npos);
}

void HttplibHttpdTests::testRunHandlerGet()
{
    httplib::Client client("127.0.0.1", _port);
    auto res = client.Get("/run?value=http-test-noop");
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT_EQUAL(200, res->status);
}

void HttplibHttpdTests::testRunHandlerPostBody()
{
    httplib::Client client("127.0.0.1", _port);
    auto res = client.Post("/run?value=http-test-echo-args",
                           R"({"children":[{"name":"marker","value":"POST"},{"name":"number","value":101}]})",
                           "application/json");
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT_EQUAL(200, res->status);
    CPPUNIT_ASSERT_EQUAL(std::string("POST"), std::string(fgGetString("/test/http/run-echo/marker")));
    CPPUNIT_ASSERT_EQUAL(101, fgGetInt("/test/http/run-echo/number"));
}

void HttplibHttpdTests::testRunHandlerPutBody()
{
    httplib::Client client("127.0.0.1", _port);
    auto res = client.Put("/run?value=http-test-echo-args",
                          R"({"children":[{"name":"marker","value":"PUT"},{"name":"number","value":102}]})",
                          "application/json");
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT_EQUAL(200, res->status);
    CPPUNIT_ASSERT_EQUAL(std::string("PUT"), std::string(fgGetString("/test/http/run-echo/marker")));
    CPPUNIT_ASSERT_EQUAL(102, fgGetInt("/test/http/run-echo/number"));
}

void HttplibHttpdTests::testRunHandlerPatchBody()
{
    httplib::Client client("127.0.0.1", _port);
    auto res = client.Patch("/run?value=http-test-echo-args",
                            R"({"children":[{"name":"marker","value":"PATCH"},{"name":"number","value":103}]})",
                            "application/json");
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT_EQUAL(200, res->status);
    CPPUNIT_ASSERT_EQUAL(std::string("PATCH"), std::string(fgGetString("/test/http/run-echo/marker")));
    CPPUNIT_ASSERT_EQUAL(103, fgGetInt("/test/http/run-echo/number"));
}

void HttplibHttpdTests::testRunHandlerDeleteBody()
{
    httplib::Client client("127.0.0.1", _port);
    auto res = client.Delete("/run?value=http-test-echo-args",
                             R"({"children":[{"name":"marker","value":"DELETE"},{"name":"number","value":104}]})",
                             "application/json");
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT_EQUAL(200, res->status);
    CPPUNIT_ASSERT_EQUAL(std::string("DELETE"), std::string(fgGetString("/test/http/run-echo/marker")));
    CPPUNIT_ASSERT_EQUAL(104, fgGetInt("/test/http/run-echo/number"));
}

void HttplibHttpdTests::testContentBearingVerbUnknownRouteReturns404()
{
    httplib::Client client("127.0.0.1", _port);
    auto res = client.Post("/nonexistent/path", "{}", "application/json");
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT_EQUAL(404, res->status);
}

void HttplibHttpdTests::testPkgHandlerWithoutPackageRoot()
{
    CPPUNIT_ASSERT(globals->packageRoot() == nullptr);

    httplib::Client client("127.0.0.1", _port);
    auto res = client.Get("/pkg/path");
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT_EQUAL(500, res->status);
}

void HttplibHttpdTests::testPkgHandlerWithPackageRoot()
{
    SGPath pkgRoot = SGPath(fgGetString("/sim/fg-home")) / "packages";
    simgear::Dir(pkgRoot).create(0755);
    globals->setPackageRoot(new simgear::pkg::Root(pkgRoot, "2024.1"));

    httplib::Client client("127.0.0.1", _port);
    auto res = client.Get("/pkg/path");
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT_EQUAL(200, res->status);
}

void HttplibHttpdTests::testNavdbHandler()
{
    httplib::Client client("127.0.0.1", _port);
    auto res = client.Get("/navdb/?q=airport&id=ZZZZ");
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT_EQUAL(200, res->status);

    auto j = json::parse(res->body);
    CPPUNIT_ASSERT_EQUAL(std::string("FeatureCollection"), j.at("type").get<std::string>());
    CPPUNIT_ASSERT(j.at("features").empty());
}

void HttplibHttpdTests::testFlightHistoryHandler()
{
    httplib::Client client("127.0.0.1", _port);
    auto res = client.Get("/flighthistory/track.json");
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT_EQUAL(200, res->status);

    auto j = json::parse(res->body);
    CPPUNIT_ASSERT_EQUAL(std::string("Feature"), j.at("type").get<std::string>());
}

void HttplibHttpdTests::testPropertyListenerWebsocket()
{
    fgSetString("/test/http/watched", "before");

    httplib::ws::WebSocketClient ws("ws://127.0.0.1:" + std::to_string(_port) + "/PropertyListener");
    // Must be set before connect(): WebSocketClient::connect() bakes the
    // read timeout into the socket it creates, so setting it afterwards
    // silently has no effect and read() blocks on whatever the (much
    // longer) default is instead.
    ws.set_read_timeout(2, 0);
    CPPUNIT_ASSERT(ws.connect());

    ws.send(std::string(R"({"command":"addListener","node":"/test/http/watched"})"));
    // Give the server's read loop a moment to process the command before
    // we change the value -- otherwise the change could (rarely) race
    // ahead of the listener being registered.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    fgSetString("/test/http/watched", "after");
    // PropertyChangeWebsocket::poll() throttles to /sim/http/property-
    // websocket/update-interval-secs (default 20Hz) based on
    // /sim/time/elapsed-sec. Nothing advances that outside a real main
    // loop -- TimeManager needs more than just being added to the
    // subsystem manager to actually tick it -- so bump it directly; the
    // connection's own poll thread (ticking in real time, independently
    // of this call) picks up the new value on its next iteration.
    fgSetDouble("/sim/time/elapsed-sec", fgGetDouble("/sim/time/elapsed-sec") + 1.0);

    std::string msg;
    auto result = ws.read(msg);
    CPPUNIT_ASSERT_EQUAL(httplib::ws::ReadResult::Text, result);

    auto j = json::parse(msg);
    CPPUNIT_ASSERT_EQUAL(std::string("/test/http/watched"), j.at("path").get<std::string>());
    CPPUNIT_ASSERT_EQUAL(std::string("after"), j.at("value").get<std::string>());
}

void HttplibHttpdTests::testPropertyTreeMirrorWebsocket()
{
    // A value change only propagates up to an ancestor's change listener
    // if the node has VALUE_CHANGED_UP set, which a node only inherits
    // automatically when created under an ancestor flagged
    // VALUE_CHANGED_DOWN (see simgear/props/props.cxx). In the real app,
    // main.cxx sets this on /sim so that watching/mirroring anything
    // under it works; this test's tree isn't part of that, so it must
    // set the same flag on its own root to see live changes at all --
    // this isn't a migration workaround, it's a real precondition of
    // MirrorPropertyTreeWebsocket that main.cxx happens to satisfy for
    // production use under /sim.
    globals->get_props()->getNode("/test", true)->setAttribute(SGPropertyNode::VALUE_CHANGED_DOWN, true);

    fgSetString("/test/http/mirror/foo", "bar");

    httplib::ws::WebSocketClient ws("ws://127.0.0.1:" + std::to_string(_port) + "/PropertyTreeMirror/test/http/mirror");
    ws.set_read_timeout(2, 0); // must be set before connect(), see testPropertyListenerWebsocket
    CPPUNIT_ASSERT(ws.connect());

    // Initial subtree snapshot.
    std::string msg;
    CPPUNIT_ASSERT_EQUAL(httplib::ws::ReadResult::Text, ws.read(msg));
    auto initial = json::parse(msg);
    CPPUNIT_ASSERT(initial.contains("created"));
    bool sawFoo = false;
    for (const auto& node : initial.at("created")) {
        if (node.at("path").get<std::string>() == "/test/http/mirror/foo") {
            sawFoo = true;
            CPPUNIT_ASSERT_EQUAL(std::string("bar"), node.at("value").get<std::string>());
        }
    }
    CPPUNIT_ASSERT(sawFoo);

    // A subsequent change is reported too.
    fgSetString("/test/http/mirror/foo", "baz");
    FGTestApi::runForTime(0.5);

    CPPUNIT_ASSERT_EQUAL(httplib::ws::ReadResult::Text, ws.read(msg));
    auto changed = json::parse(msg);
    CPPUNIT_ASSERT(changed.contains("changed"));
}
