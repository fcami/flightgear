// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "test_JsonUriHandler.hxx"
#include "httpTestHelpers.hxx"

#include "test_suite/FGTestApi/testGlobals.hxx"

#include <Main/fg_props.hxx>
#include <Network/http/HTTPResponse.hxx>
#include <Network/http/JsonUriHandler.hxx>

#include <nlohmann/json.hpp>

using namespace flightgear::http;
using namespace FGTestApi::http;

void JsonUriHandlerTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("JsonUriHandler");
}

void JsonUriHandlerTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}

// ---------------------------------------------------------------------------

void JsonUriHandlerTests::testGetExistingProperty()
{
    fgSetString("/test/aircraft", "c172p");

    JsonUriHandler handler("/json/");
    auto req = buildRequest("GET", "/json/test/aircraft");
    HTTPResponse resp;

    bool handled = handler.handleRequest(req, resp, nullptr);
    CPPUNIT_ASSERT(handled);
    CPPUNIT_ASSERT_EQUAL(200, resp.StatusCode);

    auto json = nlohmann::json::parse(resp.Content);
    CPPUNIT_ASSERT_EQUAL(std::string("c172p"), json.at("value").get<std::string>());
    CPPUNIT_ASSERT_EQUAL(std::string("/test/aircraft"), json.at("path").get<std::string>());
}

void JsonUriHandlerTests::testGetMissingProperty()
{
    JsonUriHandler handler("/json/");
    auto req = buildRequest("GET", "/json/this/does/not/exist");
    HTTPResponse resp;

    bool handled = handler.handleRequest(req, resp, nullptr);
    CPPUNIT_ASSERT(handled);
    CPPUNIT_ASSERT_EQUAL(404, resp.StatusCode);
}

void JsonUriHandlerTests::testPostProperty()
{
    fgSetString("/test/writable", "original");

    JsonUriHandler handler("/json/");
    // The JSON body uses {"value":...} to update the node at the URI directly
    auto req = buildRequest("POST", "/json/test/writable", R"({"value":"updated"})");
    HTTPResponse resp;

    bool handled = handler.handleRequest(req, resp, nullptr);
    CPPUNIT_ASSERT(handled);
    CPPUNIT_ASSERT_EQUAL(200, resp.StatusCode);
    CPPUNIT_ASSERT_EQUAL(std::string("updated"), std::string(fgGetString("/test/writable")));
}
