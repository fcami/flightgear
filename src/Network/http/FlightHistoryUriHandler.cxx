// FlightHistoryUriHandler.cxx -- FlightHistory service
//
// Written by Torsten Dreyer, started February 2015.
//
// SPDX-FileCopyrightText: 2015 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later

#include "FlightHistoryUriHandler.hxx"
#include "SimpleDOM.hxx"
#include "simgear/debug/debug_types.h"

#include <Aircraft/FlightHistory.hxx>
#include <Main/fg_props.hxx>
#include <nlohmann/json.hpp>
#include <sstream>

using std::string;
using std::stringstream;

namespace flightgear {
namespace http {

/*
{
  type: "Feature",
  geometry: {
    type: "LineString",
    coordinates: [lon,lat,alt],..]
  },
  properties: {
    type: "FlightHistory"
  },
}
*/

static const char* errorPage =
    "<html><head><title>Flight History</title></head><body>"
    "<h1>Flight History</h1>"
    "Supported formats:"
    "<ul>"
    "<li><a href='track.json'>JSON</a></li>"
    "<li><a href='track.kml'>KML</a></li>"
    "</ul>"
    "</body></html>";

static string FlightHistoryToJson(const SGGeodVec& history, size_t last_seen)
{
    using nj = nlohmann::json;
    auto lineArray = nj{};
    for (const auto& g : history) {
        lineArray.push_back(nj{g.getLongitudeDeg(), g.getLatitudeDeg(), g.getElevationM()});
    }

    auto feature = nj{
        {"type", "Feature"},
        {"properties", nj{
                           {"type", "FlightHistory"},
                           {"last", last_seen}}},
        {"geometry", nj{{"type", "LineString"}, {"coordinates", lineArray}}}};

    return feature.dump();
}

static string AutoUpdateResponse(const HTTPRequest& request,
                                 const string& base, const string& interval)
{
    string url = "http://";
    url.append(request.HeaderVariables.get("Host")).append(base);

    std::string reply("<?xml version='1.0' encoding='UTF-8'?>");
    DOMNode* kml = new DOMNode("kml");
    kml->setAttribute("xmlns", "http://www.opengis.net/kml/2.2");

    DOMNode* document = kml->addChild(new DOMNode("Document"));

    DOMNode* networkLink = document->addChild(new DOMNode("NetworkLink"));
    DOMNode* link = networkLink->addChild(new DOMNode("Link"));
    link->addChild(new DOMNode("href"))->addChild(new DOMTextElement(url));
    link->addChild(new DOMNode("refreshMode"))->addChild(new DOMTextElement("onInterval"));
    link->addChild(new DOMNode("refreshInterval"))->addChild(new DOMTextElement(interval.empty() ? "10" : interval));

    reply.append(kml->render());
    delete kml;
    return reply;
}

static string FlightHistoryToKml(const SGGeodVec& history,
                                 const HTTPRequest& request)
{
    string interval = request.RequestVariables.get("interval");
    if (!interval.empty()) {
        return AutoUpdateResponse(request, "/flighthistory/track.kml", interval);
    }

    std::string reply("<?xml version='1.0' encoding='UTF-8'?>");
    DOMNode* kml = new DOMNode("kml");
    kml->setAttribute("xmlns", "http://www.opengis.net/kml/2.2");

    DOMNode* document = kml->addChild(new DOMNode("Document"));

    document->addChild(new DOMNode("name"))->addChild(new DOMTextElement("FlightGear"));
    document->addChild(new DOMNode("description"))->addChild(new DOMTextElement("FlightGear Flight History"));

    DOMNode* style = document->addChild(new DOMNode("Style"))->setAttribute("id", "flight-history");
    DOMNode* lineStyle = style->addChild(new DOMNode("LineStyle"));

    string lineColor = request.RequestVariables.get("LineColor");
    string lineWidth = request.RequestVariables.get("LineWidth");
    string polyColor = request.RequestVariables.get("PolyColor");

    lineStyle->addChild(new DOMNode("color"))->addChild(new DOMTextElement(lineColor.empty() ? "427ebfff" : lineColor));
    lineStyle->addChild(new DOMNode("width"))->addChild(new DOMTextElement(lineWidth.empty() ? "4" : lineWidth));

    lineStyle = style->addChild(new DOMNode("PolyStyle"));
    lineStyle->addChild(new DOMNode("color"))->addChild(new DOMTextElement(polyColor.empty() ? "fbfc4600" : polyColor));

    DOMNode* placemark = document->addChild(new DOMNode("Placemark"));
    placemark->addChild(new DOMNode("name"))->addChild(new DOMTextElement("Flight Path"));
    placemark->addChild(new DOMNode("styleUrl"))->addChild(new DOMTextElement("#flight-history"));

    DOMNode* linestring = placemark->addChild(new DOMNode("LineString"));
    linestring->addChild(new DOMNode("extrude"))->addChild(new DOMTextElement("1"));
    linestring->addChild(new DOMNode("tessalate"))->addChild(new DOMTextElement("1"));
    linestring->addChild(new DOMNode("altitudeMode"))->addChild(new DOMTextElement("absolute"));

    stringstream ss;

    for (SGGeodVec::const_iterator it = history.begin(); it != history.end();
         ++it) {
        ss << (*it).getLongitudeDeg() << "," << (*it).getLatitudeDeg() << ","
           << it->getElevationM() << " ";
    }

    linestring->addChild(new DOMNode("coordinates"))->addChild(new DOMTextElement(ss.str()));

    reply.append(kml->render());
    delete kml;
    return reply;
}

bool FlightHistoryUriHandler::handleRequest(const HTTPRequest& request,
                                            HTTPResponse& response, Connection* connection)
{
    using nj = nlohmann::json;

    response.Header["Access-Control-Allow-Origin"] = "*";
    response.Header["Access-Control-Allow-Methods"] = "OPTIONS, GET, POST";
    response.Header["Access-Control-Allow-Headers"] =
        "Origin, Accept, Content-Type, X-Requested-With, X-CSRF-Token";

    if (request.Method == "OPTIONS") {
        return true; // OPTIONS only needs the headers
    }

    auto history = globals->get_subsystem<FGFlightHistory>();
    if (!history) {
        response.Header["Content-Type"] = "text/html";
        response.Content = errorPage;
        return true;
    }

    double minEdgeLengthM = 50;
    string requestPath = request.Uri.substr(getUri().length());

    if (request.Method == "GET") {
    } else if (request.Method == "POST") {
        /*
         * {
         *   sampleIntervalSec: (number),
         *   maxMemoryUseBytes: (number),
         *   clearOnTakeoff: (bool),
         *   enabled: (bool),
         * }
         */
        auto json = nj::parse(request.Content, nullptr, false);
        if (json.is_discarded()) {
            SG_LOG(SG_NETWORK, SG_DEV_WARN, "Failed to parse request JSON:" << request.Content);
            return false;
        }

        if (json.contains("sampleIntervalSec")) {
            fgSetDouble("/sim/history/sample-interval-sec", json.value<double>("sampleIntervalSec", 1.0));
        }

        if (json.contains("maxMemoryUseBytes")) {
            fgSetDouble("/sim/history/max-memory-use-bytes", json.value<double>("maxMemoryUseBytes", 1.0));
        }

        if (json.contains("clearOnTakeoff")) {
            fgSetBool("/sim/history/clear-on-takeoff", json.value<bool>("clearOnTakeoff", false));
        }

        if (json.contains("enabled")) {
            fgSetBool("/sim/history/enabled", json.value<bool>("enabled", false));
        }

        response.Content = "{}";
        return true;

    } else {
        SG_LOG(SG_NETWORK, SG_INFO,
               "FlightHistoryUriHandler: invalid request method '" << request.Method << "'");
        response.Header["Allow"] = "OPTIONS, GET, POST";
        response.StatusCode = 405;
        response.Content = "{}";
        return true;
    }

    if (requestPath == "track.kml") {
        response.Header["Content-Type"] =
            "application/vnd.google-earth.kml+xml; charset=UTF-8";

        response.Content = FlightHistoryToKml(
            history->pathForHistory(minEdgeLengthM), request);

    } else if (requestPath == "track.json") {
        size_t count = -1;
        try {
            count = std::stoul(request.RequestVariables.get("count"));
        } catch (...) {
        }
        size_t last = 0;
        try {
            last = std::stoul(request.RequestVariables.get("last"));
        } catch (...) {
        }

        response.Header["Content-Type"] = "application/json; charset=UTF-8";
        PagedPathForHistory_ptr h = history->pagedPathForHistory(count, last);
        response.Content = FlightHistoryToJson(h->path, h->last_seen);

    } else {
        response.Header["Content-Type"] = "text/html";
        response.Content = errorPage;
    }

    return true;
}

} // namespace http
} // namespace flightgear
