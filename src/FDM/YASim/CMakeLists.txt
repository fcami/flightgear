# SPDX-FileCopyrightText: 2010 James Turner
# SPDX-License-Identifier: GPL-2.0-or-later

set(COMMON
	Airplane.cpp
	YASimAtmosphere.cpp
	ControlMap.cpp
	ElectricEngine.cpp
	FGFDM.cpp
	Gear.cpp
	Glue.cpp
	Ground.cpp
	Hitch.cpp
	Hook.cpp
	Integrator.cpp
	Jet.cpp
	Launchbar.cpp
	Model.cpp
	PistonEngine.cpp
	PropEngine.cpp
	Propeller.cpp
	RigidBody.cpp
	Rotor.cpp
	Rotorpart.cpp
	SimpleJet.cpp
	Surface.cpp
	TurbineEngine.cpp
	Turbulence.cpp
	Wing.cpp
	Version.cpp
	yasim-common.cpp
	)

set(SOURCES
	${COMMON}
	YASim.cxx
	FGGround.cpp
	)

target_sources(fgfsObjects PRIVATE ${SOURCES})

add_executable(yasim yasim-test.cpp ${COMMON})
add_executable(yasim-proptest proptest.cpp ${COMMON})

target_link_libraries(yasim SimGearCore)
target_link_libraries(yasim-proptest SimGearCore)

install(TARGETS yasim yasim-proptest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
