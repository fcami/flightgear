include(FlightGearComponent)

set(SOURCES
	MQTTSystem.cxx
	MQTTClient.cxx
	StringInterpolation.cxx
	)

set(HEADERS
	MQTTClient.hxx
	MQTTSystem.hxx
	StringInterpolation.hxx
	)

flightgear_component(Network "${SOURCES}" "${HEADERS}")
