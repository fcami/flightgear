# SPDX-FileCopyrightText: 2014 Torsten Dreyer
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfsObjects PRIVATE
	httpd.cxx
	ScreenshotUriHandler.cxx
	PropertyUriHandler.cxx
	JsonUriHandler.cxx
    FlightHistoryUriHandler.cxx
	PkgUriHandler.cxx
	RunUriHandler.cxx
	MirrorPropertyTreeWebsocket.cxx
	NavdbUriHandler.cxx
	PropertyChangeWebsocket.cxx
	PropertyChangeObserver.cxx
	jsonprops.cxx
	SimpleDOM.cxx
	urihandler.hxx
	httpd.hxx
	ScreenshotUriHandler.hxx
	PropertyUriHandler.hxx
	JsonUriHandler.hxx
    FlightHistoryUriHandler.hxx
	PkgUriHandler.hxx
	RunUriHandler.hxx
	NavdbUriHandler.hxx
	HTTPRequest.hxx
	Websocket.hxx
	PropertyChangeWebsocket.hxx
	PropertyChangeObserver.hxx
	MirrorPropertyTreeWebsocket.hxx
	jsonprops.hxx
    SimpleDOM.hxx
)
