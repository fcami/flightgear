// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2007 Durk Talsma

#include "config.h"

#include <cstdio>
#include <utility>

#include <simgear/misc/sg_path.hxx>
#include <simgear/xml/easyxml.hxx>
#include <simgear/misc/strutils.hxx>
#include <simgear/timing/timestamp.hxx>

#include <Main/globals.hxx>
#include <Main/fg_props.hxx>

#include "xmlloader.hxx"
#include "dynamicloader.hxx"
#include "runwayprefloader.hxx"

#include "dynamics.hxx"
#include "airport.hxx"
#include "runwayprefs.hxx"

#include <Navaids/NavDataCache.hxx>
#include <Main/sentryIntegration.hxx>

using std::string;

XMLLoader::XMLLoader() {}
XMLLoader::~XMLLoader() {}

void XMLLoader::load(FGGroundNetwork* net)
{
  SGPath path;
  if (!findAirportData(net->airport()->ident(), "groundnet", path)) {
    return;
  }

  SG_LOG(SG_NAVAID, SG_DEBUG, "reading groundnet data from " << path);
  SGTimeStamp t;
  t.stamp();
  try {
      FGGroundNetXMLLoader visitor(net);
      readXML(path, visitor);

      if (visitor.hasErrors() && fgGetBool("/sim/terrasync/enabled")) {
          flightgear::updateSentryTag("ground-net", net->airport()->ident());
          
          // disabled for 2024.1.4 to reduce Sentry quota spam
          //flightgear::sentryReportException("Ground-net load error", path.utf8Str());
      }
  } catch (sg_exception& e) {
    SG_LOG(SG_NAVAID, SG_DEV_WARN, "parsing groundnet XML failed:" << e.getFormattedMessage());
  }

  SG_LOG(SG_NAVAID, SG_DEBUG, "parsing groundnet XML took " << t.elapsedMSec());
}

void XMLLoader::loadFromStream(FGGroundNetwork* net, std::istream& inData)
{
  try {
      FGGroundNetXMLLoader visitor(net);
      readXML(inData, visitor);

      if (visitor.hasErrors()) {
          // disabled for 2024.1.4 to reduce Sentry quota spam
          // flightgear::sentryReportException("Ground-net load error", {});
      }
  } catch (sg_exception& e) {
    SG_LOG(SG_NAVAID, SG_DEV_WARN, "parsing groundnet XML failed:" << e.getFormattedMessage());
  }
}

void XMLLoader::loadFromPath(FGGroundNetwork* net, const SGPath& path)
{
  try {
      FGGroundNetXMLLoader visitor(net);
      readXML(path, visitor);

      if (visitor.hasErrors() && fgGetBool("/sim/terrasync/enabled")) {
          flightgear::updateSentryTag("ground-net", net->airport()->ident());
          // disabled for 2024.1.4 to reduce Sentry quota spam
          // flightgear::sentryReportException("Ground-net load error", path.utf8Str());
      }
  } catch (sg_exception& e) {
    SG_LOG(SG_NAVAID, SG_DEV_WARN, "parsing groundnet XML failed:" << e.getFormattedMessage());
  }
}

void XMLLoader::load(FGRunwayPreference* p) {
  FGRunwayPreferenceXMLLoader visitor(p);
  loadAirportXMLDataIntoVisitor(p->getId(), "rwyuse", visitor);
}

bool XMLLoader::findAirportData(const std::string& aICAO,
                                const std::string& aFileName, SGPath& aPath)
{
  FGAirportRef airport = FGAirport::findByIdent(aICAO);
  if (!airport) {
      return false;
  }

  string fileName(aFileName);
  if (!simgear::strutils::ends_with(aFileName, ".xml")) {
    fileName.append(".xml");
  }

  const bool performFullTraversal = airport->sceneryPath().isNull() ||
    !fileName.compare("procedures.xml");

  PathList sc = globals->get_fg_scenery();
  char buffer[128];
  ::snprintf(buffer, 128, "%c/%c/%c/%s.%s",
             aICAO[0], aICAO[1], aICAO[2],
             aICAO.c_str(), fileName.c_str());

  for (PathList::const_iterator it = sc.begin(); it != sc.end(); ++it) {
      // fg_senery contains empty strings as "markers" (see FGGlobals::set_fg_scenery)
      if (!it->isNull()) {
          const SGPath path = *it / "Airports" / buffer;
          if (path.exists()) {
              aPath = std::move(path);
              return true;
          } // of path exists

          // Unless we are in “full traversal mode”, don't look in scenery paths
          // that come after the one which contributed the apt.dat file for the
          // airport.
          if (!performFullTraversal && *it == airport->sceneryPath()) {
              return false;
          }
      }
  } // of scenery path iteration
  return false;
}

bool XMLLoader::loadAirportXMLDataIntoVisitor(const string& aICAO,
                                              const string& aFileName, XMLVisitor& aVisitor)
{
  SGPath path;
  if (!findAirportData(aICAO, aFileName, path)) {
    SG_LOG(SG_NAVAID, SG_DEBUG, "loadAirportXMLDataIntoVisitor: failed to find data for " << aICAO << "/" << aFileName);
    return false;
  }

    bool readXMLOk = true;
    try {
        SG_LOG(SG_NAVAID, SG_DEBUG, "loadAirportXMLDataIntoVisitor: loading from " << path);
        readXML(path, aVisitor);
    } catch (sg_exception& e) {
        readXMLOk = false;
        SG_LOG(SG_NAVAID, SG_WARN, "XML errors trying to read:" << path);
    }

    return readXMLOk;
}
