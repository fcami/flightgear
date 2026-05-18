# SPDX-FileCopyrightText: 2019 Lars Toenning
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfsObjects PRIVATE
	swift_connection.cxx
	dbusconnection.cpp
	dbusobject.cpp
	dbusmessage.cpp
	dbusdispatcher.cpp
	dbuserror.cpp
	dbusserver.cpp
	plugin.cpp
	service.cpp
	traffic.cpp
	SwiftAircraftManager.cpp
	swift_connection.hxx
	dbusconnection.h
	dbusobject.h
	dbusmessage.h
	dbuscallbacks.h
	dbusdispatcher.h
	dbuserror.h
	dbusserver.h
	plugin.h
	service.h
	traffic.h
	SwiftAircraftManager.h
)
