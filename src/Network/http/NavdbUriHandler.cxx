// NavdbUriHandler.cxx -- Access the nav database
//
// SPDX-FileCopyrightText: 2014  Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later


#include "NavdbUriHandler.hxx"
#include "Navaids/positioned.hxx"
#include <ATC/CommStation.hxx>
#include <Airports/airport.hxx>
#include <Navaids/navrecord.hxx>
#include <simgear/debug/logstream.hxx>

#include <nlohmann/json.hpp>

using std::string;

using njson = nlohmann::json;

namespace flightgear {
namespace http {

static njson createPositionArray(double x, double y, double z)
{
    return njson{x, y, z};
}

static njson createPositionArray(double x, double y)
{
    return njson{x, y};
}

static njson createLOCGeometry(FGNavRecord* navRecord)
{
  assert( navRecord != NULL );

  njson coordArray;

  const int range = navRecord->get_range();
  const double width = navRecord->localizerWidth();
  const double course = navRecord->get_multiuse();

  double px[4];
  double py[4];

  px[0] = navRecord->longitude();
  py[0] = navRecord->latitude();

  for (int i = -1; i <= +1; i++) {
    double c = SGMiscd::normalizeAngle((course + 180 + i * width / 2) * SG_DEGREES_TO_RADIANS);
    SGGeoc geoc = SGGeoc::fromGeod(navRecord->geod());
    SGGeod p2 = SGGeod::fromGeoc(geoc.advanceRadM(c, range * SG_NM_TO_METER));
    px[i + 2] = p2.getLongitudeDeg();
    py[i + 2] = p2.getLatitudeDeg();

    njson line;
    line.push_back(createPositionArray(px[0], py[0]));
    line.push_back(createPositionArray(px[i + 2], py[i + 2]));
    coordArray.push_back(line);
  }
  return njson{
      {"type", "MultiLineString"},
      {"coordinates", coordArray}};
}

static njson createPointGeometry(FGPositioned* positioned)
{
    return njson{
        {"type", "Point"},
        {"coordinates", createPositionArray(positioned->longitude(), positioned->latitude(), positioned->elevationM())}};
}

static njson createRunwayPolygon(FGRunwayBase* rwy)
{
  // compute the four corners of the runway
  SGGeod p1 = rwy->pointOffCenterline( 0.0, rwy->widthM()/2 );
  SGGeod p2 = rwy->pointOffCenterline( 0.0, -rwy->widthM()/2 );
  SGGeod p3 = rwy->pointOffCenterline( rwy->lengthM(), -rwy->widthM()/2 );
  SGGeod p4 = rwy->pointOffCenterline( rwy->lengthM(), rwy->widthM()/2 );

  njson linearRing{
      createPositionArray(p1.getLongitudeDeg(), p1.getLatitudeDeg()),
      createPositionArray(p2.getLongitudeDeg(), p2.getLatitudeDeg()),
      createPositionArray(p3.getLongitudeDeg(), p3.getLatitudeDeg()),
      createPositionArray(p4.getLongitudeDeg(), p4.getLatitudeDeg()),
      createPositionArray(p1.getLongitudeDeg(), p1.getLatitudeDeg())};

  njson coordArray;
  coordArray.push_back(linearRing);

  return njson{
      {"type", "Polygon"},
      {"coordinates", coordArray}};
}

static njson createAirportGeometry(FGAirport* airport)
{
  assert( airport != NULL );
  const auto& runways = airport->getRunwaysWithoutReciprocals();

  if( runways.empty() ) {
    // no runways? Create a Point geometry
    return createPointGeometry( airport );
  }

  njson geometryArray;

  // the first item is the aerodrome reference point
  geometryArray.push_back(createPointGeometry(airport));

  // followed by the runway polygons
  for (auto rwy : runways) {
      geometryArray.push_back(createRunwayPolygon(rwy));
  }

  // followed by the taxiway polygons
  for (auto tx : airport->getTaxiways()) {
      geometryArray.push_back(createRunwayPolygon(tx));
  }

  return njson{
      {"type", "GeometryCollection"},
      {"geometries", geometryArray}};
}

static njson createGeometryFor(FGPositioned* positioned)
{
  switch( positioned->type() ) {
    case FGPositioned::LOC:
    case FGPositioned::ILS:
      return createLOCGeometry( dynamic_cast<FGNavRecord*>(positioned) );

    case FGPositioned::AIRPORT:
      return createAirportGeometry( dynamic_cast<FGAirport*>(positioned) );

    default:
      return createPointGeometry( positioned );
  }
}

static void addAirportProperties(njson& json, FGAirport* airport)
{
    if (!airport) return;

    json["name"] = airport->getName();
    njson runways;

    for (auto rwy : airport->getRunways()) {
        njson rj{
            {"id", rwy->ident()},
            {"length_m", rwy->lengthM()},
            {"width_m", rwy->widthM()},
            {"surface", rwy->surfaceName()},
            {"heading_deg", rwy->headingDeg()}};

        if (rwy->displacedThresholdM() > 0.0) {
            rj["dispacedThreshold_m"] = rwy->displacedThresholdM();
        }

        if (rwy->stopwayM() > 0.0) {
            rj["stopway_m"] = rwy->stopwayM();
        }

        runways.push_back(rj);
    }

    json["runways"] = runways;

    auto longestRunway = airport->longestRunway();
    json["longestRwyLength_m"] = longestRunway->lengthM();
    json["longestRwyHeading_deg"] = longestRunway->headingDeg();
    json["longestRwySurface"] = longestRunway->surfaceName();

    if (airport->getMetar()) {
        json["metar"] = true;
    }

    njson comms;
    for (auto c : airport->commStations()) {
        comms.push_back({{"id", c->ident()},
                         {"mhz", c->freqMHz()}});
    }

    json["comm"] = comms;
}

static void addNAVProperties(njson& json, FGNavRecord* navRecord)
{
    if (!navRecord) return;

    json["range_nm"] = navRecord->get_range();
    json["frequency"] = navRecord->get_freq() / 100.0;

    switch (navRecord->type()) {
    case FGPositioned::ILS:
    case FGPositioned::LOC:
        json["localizer-course"] = navRecord->get_multiuse();
        break;

    case FGPositioned::VOR:
        json["variation"] = navRecord->get_multiuse();
        break;

    default:
      break;
  }
}

static njson createPropertiesFor(FGPositioned* positioned)
{
    njson props{
        {"name", positioned->name()},
        {"id", positioned->ident()},
        {"type", positioned->typeString()},
        {"elevation-m", positioned->elevationM()}};

    addNAVProperties(props, fgpositioned_cast<FGNavRecord>(positioned));
    addAirportProperties(props, fgpositioned_cast<FGAirport>(positioned));

    return props;
}

static njson createFeatureFor(FGPositioned* positioned)
{
    return njson{
        {"type", "Feature"},
        {"geometry", createGeometryFor(positioned)},
        {"properties", createPropertiesFor(positioned)},
        {"id", positioned->ident()}};
}

bool NavdbUriHandler::handleRequest(const HTTPRequest & request, HTTPResponse & response, Connection * connection)
{

  response.Header["Content-Type"] = "application/json; charset=UTF-8";
  response.Header["Access-Control-Allow-Origin"] = "*";
  response.Header["Access-Control-Allow-Methods"] = "OPTIONS, GET";
  response.Header["Access-Control-Allow-Headers"] = "Origin, Accept, Content-Type, X-Requested-With, X-CSRF-Token";

  if( request.Method == "OPTIONS" ){
      return true; // OPTIONS only needs the headers
  }

  if( request.Method != "GET" ){
    response.Header["Allow"] = "OPTIONS, GET";
    response.StatusCode = 405;
    response.Content = "{}";
    return true;
  }

  bool indent = request.RequestVariables.get("i") == "y";

  string query = request.RequestVariables.get("q");
  FGPositionedList result;

  if (query == "findWithinRange") {
    // ?q=findWithinRange&lat=53.5&lon=10.0&range=100&type=vor,ils

    double lat, lon, range = -1;
    try {
      lat = std::stod(request.RequestVariables.get("lat"));
      lon = std::stod(request.RequestVariables.get("lon"));
      range = std::stod(request.RequestVariables.get("range"));
    }
    catch (...) {
      goto fail;
    }

    if (range <= 1.0) goto fail;
    // In remembrance of a famous bug

    SGGeod pos = SGGeod::fromDeg(lon, lat);
    FGPositioned::TypeFilter filter;
    try {
      filter = FGPositioned::TypeFilter::fromString(request.RequestVariables.get("type"));
    }
    catch (...) {
      goto fail;
    }

    result = FGPositioned::findWithinRange(pos, range, &filter);
  } else if (query == "airports") {
      // until we decide if re-implementing this API makes sense on the NavDB,
      // let's return a 404 to avoid confusion.
      response.StatusCode = 404;
      response.Content = "{\"error\": \"currently unimplemented\"}"; // valid JSON
      return true;
  } else if (query == "airport") {
    FGAirportRef airport = FGAirport::findByIdent(request.RequestVariables.get("id"));
    if( airport.valid() )
      result.push_back( airport );
  } else {
    goto fail;
  }

 { // create some GeoJSON from the result list
     // Each element in the array is a feature object
     njson features;

     for (auto p : result) {
         features.push_back(createFeatureFor(p));
     }

     // GeoJSON always consists of a single object.
     njson geoJSON{
         // The GeoJSON object must have a member with the name "type".
         // This member's value is a string that determines the type of the GeoJSON object.
         {"type", "FeatureCollection"},
         {"features", features}};

     response.Content = geoJSON.dump(indent ? 2 : -1);
  }

  return true;

  fail: response.StatusCode = 400;
  response.Content = "{ 'error': 'bad request' }";
  return true;
}

} // namespace http
} // namespace flightgear
