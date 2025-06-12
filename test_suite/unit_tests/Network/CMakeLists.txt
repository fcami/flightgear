

if (ENABLE_SWIFT)
        set(SWIFT_TESTS_SOURCES
                ${CMAKE_CURRENT_SOURCE_DIR}/test_swiftService.cxx
                ${CMAKE_CURRENT_SOURCE_DIR}/test_swiftAircraftManager.cxx
                )

        set(SWIFT_TESTS_HEADERS
                ${CMAKE_CURRENT_SOURCE_DIR}/test_swiftService.hxx
                ${CMAKE_CURRENT_SOURCE_DIR}/test_swiftAircraftManager.hxx
                )
endif()

if (ENABLE_MQTT)
        set(MQTT_TESTS_SOURCES
                ${CMAKE_CURRENT_SOURCE_DIR}/test_StringInterpolation.cxx
                ${CMAKE_CURRENT_SOURCE_DIR}/test_MQTT.cxx

        )

        set(SWIFT_TESTS_HEADERS
                ${CMAKE_CURRENT_SOURCE_DIR}/test_StringInterpolation.hxx
                ${CMAKE_CURRENT_SOURCE_DIR}/test_MQTT.hxx

        )
endif()


set(TESTSUITE_SOURCES
        ${TESTSUITE_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/TestSuite.cxx
        ${SWIFT_TESTS_SOURCES}
        ${MQTT_TESTS_SOURCES}
        PARENT_SCOPE
        )

set(TESTSUITE_HEADERS
        ${TESTSUITE_HEADERS}
        ${SWIFT_TESTS_HEADERS}
        ${MQTT_TESTS_HEADERS}
        PARENT_SCOPE
        )

