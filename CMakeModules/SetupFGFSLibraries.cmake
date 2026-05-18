# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: James Turner <james@flightgear.org>
function(setup_fgfs_libraries target)
    if(ENABLE_JSBSIM)
        target_link_libraries(${target} PRIVATE JSBSim)
    endif()

    if(ENABLE_IAX)
        target_link_libraries(${target} PRIVATE iaxclient_lib)
    endif()

    if(HAVE_DBUS)
        target_link_libraries(${target} PRIVATE DBus::DBus)
    endif()

    if(X11_FOUND)
        target_link_libraries(${target} PRIVATE ${X11_LIBRARIES})
    endif()

    if(ENABLE_OSGXR)
        target_link_libraries(${target} PRIVATE osgXR)
    endif()

    target_link_libraries(${target} PRIVATE fgsqlite3 fgvoicesynth fgembeddedresources FGTHinksFMM)

    target_link_libraries(${target} PRIVATE
        SimGearCore
        SimGearScene
        Boost::boost
        ${EVENT_INPUT_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${OPENSCENEGRAPH_LIBRARIES}
        ${PLATFORM_LIBS}
    )

    if (ENABLE_SWIFT)
        target_link_libraries(${target} PRIVATE DBus::DBus libEvent::libEvent)
    endif()

    if (ENABLE_PLIB_JOYSTICK)
        target_link_libraries(${target} PRIVATE PLIBJoystick)
    endif()

    if (HAVE_QT)
        target_link_libraries(${target} PRIVATE fglauncher fgqmlui)
    endif()

    if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
        target_link_libraries(${target} PRIVATE execinfo)
    endif()

    if(${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
        target_link_libraries(${target} PRIVATE execinfo)
    endif()

    if (TARGET sentry::sentry)
        target_link_libraries(${target} PRIVATE sentry::sentry)
    endif()

    target_link_libraries(${target} PRIVATE nlohmann_json::nlohmann_json)
endfunction()
