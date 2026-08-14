// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear 1.0

// Pill-shaped '+' button shown after the last selected-tag lozenge; opens
// the full tag list so the user can add another tag.
Rectangle {
    id: root

    signal clicked()

    height: icon.implicitHeight + Style.margin
    width: height
    radius: height / 2
    color: mouseArea.containsMouse ? Style.activeColor : Style.themeColor

    Image {
        id: icon
        anchors.centerIn: parent
        source: "image://colored-icon/add-small?themeContrast"
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.clicked()
    }
}
