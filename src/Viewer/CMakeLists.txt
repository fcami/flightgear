# SPDX-FileCopyrightText: 2010 James Turner <james@flightgear.org>
# SPDX-License-Identifier: GPL-2.0-or-later

include(FlightGearComponent)

set(SOURCES
	CameraGroup.cxx
	FGEventHandler.cxx
	FGMouseCursor3D.cxx
	FGDirectionCue3D.cxx
	WindowBuilder.cxx
	WindowSystemAdapter.cxx
	fg_os_osgviewer.cxx
	fgviewer.cxx
	ViewPropertyEvaluator.cxx
	renderer.cxx
	splash.cxx
	viewmgr.cxx
	sview.cxx
	GraphicsPresets.cxx
	PluginLoader.cxx
	)

set(HEADERS
	CameraGroup.hxx
	FGEventHandler.hxx
	FGMouseCursor3D.hxx
	FGDirectionCue3D.hxx
	WindowBuilder.hxx
	WindowSystemAdapter.hxx
	fgviewer.hxx
	renderer.hxx
	splash.hxx
	view.hxx
	viewmgr.hxx
	sview.hxx
	GraphicsPresets.hxx
	VRManager.hxx
	)

if (ENABLE_OSGXR)
	list(APPEND SOURCES
		VRManager.cxx
		)
endif()

# view.cxx is compiled differently for testing
flightgear_component(Viewer "${SOURCES}" "${HEADERS}" view.cxx)
