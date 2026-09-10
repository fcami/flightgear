// FlightHistoryUriHandler.hxx -- FlightHistory service
//
// Written by Torsten Dreyer, started February 2015.
//
// SPDX-FileCopyrightText: 2015 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "urihandler.hxx"
#include <simgear/props/props.hxx>

namespace flightgear {
namespace http {

class FlightHistoryUriHandler : public URIHandler {
public:
	FlightHistoryUriHandler( const std::string& uri = "/flighthistory/" ) : URIHandler( uri  ) {}
  virtual bool handleRequest( const HTTPRequest & request, HTTPResponse & response, Connection * connection );
private:
};

} // namespace http
} // namespace flightgear
