// SPDX-FileCopyrightText: 2017 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear 1.0

Rectangle {
    id: root

    property alias text: buttonText.text
    property bool active: false

    signal clicked

    width: buttonText.width + (radius * 2)
    height: buttonText.height + (radius * 2)
    radius: 6

    color: mouse.containsMouse ? Style.activeColor : (active ? Style.themeColor : Style.backgroundColor)

    StyledText {
        id: buttonText
        anchors.centerIn: parent
        color: root.enabled ? ((active | mouse.containsMouse) ? Style.themeContrastTextColor : Style.baseTextColor) : Style.disabledTextColor
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: root.enabled
        enabled: root.enabled

        onClicked: {
            root.clicked();
        }

    }
}
